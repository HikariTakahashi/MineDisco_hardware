#include "WiFiS3.h"
#include "arduino_secrets.h" 
#include "style.h" // ★ html_content.h の代わりに style.h をインクルード

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
int keyIndex = 0;

int led =  LED_BUILTIN;
int status = WL_IDLE_STATUS;
WiFiServer server(80);

// --- ★ ボタンのピン設定 ---
// 2-301室の新しい区画番号に対応するタクトスイッチ
// 新しい区画番号→ピンのマッピング: 1→2, 2→3, 4→5, 5→6, 6→7, 8→9, 10→11
// 区画3,7,9,11,12はボタンなし（リモート操作のみ）
// 新しい区画番号（1-12）をインデックス（1-12）に直接マッピング
// インデックス0は未使用、インデックス1-12が新しい区画番号1-12に対応
const int ROOM301_NEW_BOX_TO_PIN[13] = {
  0,   // インデックス0: 未使用
  2,   // インデックス1: 区画1 → ピン2
  3,   // インデックス2: 区画2 → ピン3
  0,   // インデックス3: 区画3 → ボタンなし（リモート操作のみ）
  5,   // インデックス4: 区画4 → ピン5
  6,   // インデックス5: 区画5 → ピン6（新規追加、旧7と旧16の間）
  7,   // インデックス6: 区画6 → ピン7
  0,   // インデックス7: 区画7 → ボタンなし
  9,   // インデックス8: 区画8 → ピン9
  0,   // インデックス9: 区画9 → ボタンなし（リモート操作のみ）
  11,  // インデックス10: 区画10 → ピン11
  0,   // インデックス11: 区画11 → ボタンなし（リモート操作のみ）
  0    // インデックス12: 区画12 → ボタンなし（リモート操作のみ）
};

// 旧区画番号（1-16）から新区画番号へのマッピング（302号室）
// 旧1→新1, 旧2→新2, 旧4→新3, 旧7→新5, 旧8→新6, 旧10→新7, 旧14→新4
const int ROOM302_OLD_TO_NEW[17] = {
  0,   // インデックス0: 未使用
  1,   // 旧1 → 新1
  2,   // 旧2 → 新2
  0,   // 旧3 → 削除
  3,   // 旧4 → 新3
  0,   // 旧5 → 削除
  0,   // 旧6 → 削除
  5,   // 旧7 → 新5
  6,   // 旧8 → 新6
  0,   // 旧9 → 削除
  7,   // 旧10 → 新7
  0,   // 旧11 → 削除
  0,   // 旧12 → 削除
  0,   // 旧13 → 削除
  4,   // 旧14 → 新4
  0,   // 旧15 → 削除
  0    // 旧16 → 削除
};

// 2-302室のタクトスイッチ（既存の設定を維持、必要に応じて拡張可能）
const int BTN3_PIN = 4; // 302号室: 左4
const int BTN4_PIN = 5; // 302号室: 右2

// --- リモート操作用の状態 ---
// 2-302室の16区画の状態（0=false: 非ハイライト, 1=true: ハイライト）
bool box302State[16] = {false, false, false, false, false, false, false, false,
                         false, false, false, false, false, false, false, false};

// 2-301室の17区画の状態（0=false: 非ハイライト, 1=true: ハイライト）
// インデックス0は未使用、インデックス1-12が新しい区画番号1-12に対応
// ただし、新5は旧7と旧16の間（インデックス8）に配置
bool box301State[17] = {false, false, false, false, false, false, false, false,
                         false, false, false, false, false, false, false, false, false};

// 2-301室のタクトスイッチ用のデバウンス変数（新しい区画番号に対応）
// プルダウン接続: 押していない時=LOW、押している時=HIGH
// 新しい区画番号1,2,4,5,6,8,10に対応するピンを使用
bool prevBtn301[17] = {LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW,
                        LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW};
unsigned long lastDebounceTime301[17] = {0, 0, 0, 0, 0, 0, 0, 0,
                                         0, 0, 0, 0, 0, 0, 0, 0, 0};
bool stableBtn301[17] = {LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW,
                          LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW};

// 2-302室のタクトスイッチ用のデバウンス変数
// プルダウン接続: 押していない時=LOW、押している時=HIGH
bool prevBtn3 = LOW;
bool prevBtn4 = LOW;
unsigned long lastDebounceTime3 = 0;
unsigned long lastDebounceTime4 = 0;
bool stableBtn3 = LOW;
bool stableBtn4 = LOW;

const unsigned long debounceDelay = 50; // 50ms のデバウンス時間

// --- cc:tweaked サーバー設定 ---
// cc:tweakedコンピュータのIPアドレスとポートを設定してください
// 例: IPAddress cctweaked_ip(192, 168, 1, 100);
// デフォルトは空（使用しない場合はコメントアウト）
IPAddress cctweaked_ip; // 使用する場合は setup() で設定してください
int cctweaked_port = 8080; // cc:tweakedのHTTPサーバーポート

// --- 状態変化検出用の前回の状態 ---
bool prevBox302State[16] = {false, false, false, false, false, false, false, false,
                             false, false, false, false, false, false, false, false};
bool prevBox301State[17] = {false, false, false, false, false, false, false, false,
                             false, false, false, false, false, false, false, false, false};

// --- 関数プロトタイプ ---
void printWifiStatus();
void sendDynamicPage(WiFiClient client);
void sendToCCTweaked(String room, int box, String action);
bool isCCTweakedConfigured();

void setup() {
  Serial.begin(9600);
  pinMode(led, OUTPUT);

  // ★ ピンモードを INPUT に設定（プルダウン接続: 押していない時=LOW、押している時=HIGH）
  // 2-301室の新しい区画番号に対応するタクトスイッチ（ボタンがある区画のみ）
  // 区画1,2,4,5,6,8,10に対応するピンを設定
  for (int i = 1; i <= 12; i++) {
    if (ROOM301_NEW_BOX_TO_PIN[i] != 0) {
      pinMode(ROOM301_NEW_BOX_TO_PIN[i], INPUT);
    }
  }
  // 2-302室のタクトスイッチ
  pinMode(BTN3_PIN, INPUT);
  pinMode(BTN4_PIN, INPUT);

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

    // 固定IPアドレスの設定
    IPAddress local_ip(172, 20, 10, 8);
    IPAddress gateway(172, 20, 10, 1);
    IPAddress subnet(255, 255, 255, 0);
    IPAddress dns(172, 20, 10, 1);
    
    // IPアドレスを固定する設定
    WiFi.config(local_ip, gateway, subnet, dns);

  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to Network named: ");
    Serial.println(ssid);

    status = WiFi.begin(ssid, pass);
    delay(10000);
  }
  server.begin();
  printWifiStatus();

  // --- cc:tweaked サーバーのIPアドレスを設定（必要に応じて変更してください）---
  // 例: cctweaked_ip = IPAddress(192, 168, 1, 100);
  // コメントアウトしている場合は、cc:tweakedへの送信は行われません
  // cctweaked_ip = IPAddress(192, 168, 1, 100);
}


void loop() {
  // --- トグルスイッチの状態をチェック（押した瞬間にtrue、releaseリクエストまで維持） ---
  unsigned long currentTime = millis();
  
  // 2-301室の新しい区画番号に対応するタクトスイッチを処理
  // プルダウン接続: 押していない時=LOW、押している時=HIGH
  // 新しい区画番号1-12を処理（インデックス1-12が新しい区画番号1-12に対応）
  for (int newBox = 1; newBox <= 12; newBox++) {
    int pin = ROOM301_NEW_BOX_TO_PIN[newBox];
    if (pin == 0) {
      // ボタンがない区画はスキップ
      continue;
    }
    
    bool currentBtn = digitalRead(pin);
    if (currentBtn != prevBtn301[newBox]) {
      // 状態が変化したので、デバウンスタイマーをリセット
      lastDebounceTime301[newBox] = currentTime;
    }
    // デバウンス時間が経過したら、状態を確定
    if ((currentTime - lastDebounceTime301[newBox]) > debounceDelay) {
      if (stableBtn301[newBox] != currentBtn) {
        // 押された瞬間（LOW→HIGH）のみtrueに設定（離してもtrueのまま）
        if (stableBtn301[newBox] == LOW && currentBtn == HIGH) {
          box301State[newBox] = true;
          Serial.print("BTN301[区画");
          Serial.print(newBox);
          Serial.print("] (pin ");
          Serial.print(pin);
          Serial.print(") pressed -> box301State[");
          Serial.print(newBox);
          Serial.println("] = true");
          // 状態変化検出でcc:tweakedに通知される
        }
        // 離したとき（HIGH→LOW）は状態を変更しない（releaseリクエストまで維持）
        stableBtn301[newBox] = currentBtn;
      }
    }
    prevBtn301[newBox] = currentBtn;
  }
  
  // 状態変化検出: 301号室の赤色変化を検出してcc:tweakedに送信
  for (int newBox = 1; newBox <= 12; newBox++) {
    if (box301State[newBox] != prevBox301State[newBox]) {
      if (box301State[newBox]) {
        // 赤色に変化した
        if (isCCTweakedConfigured()) {
          sendToCCTweaked("301", newBox, "set");
        }
      } else {
        // 赤色が解除された
        if (isCCTweakedConfigured()) {
          sendToCCTweaked("301", newBox, "clear");
        }
      }
      prevBox301State[newBox] = box301State[newBox];
    }
  }

  // BTN3 (302号室: 旧区画4 = 新区画3) の処理
  // プルダウン接続: 押していない時=LOW、押している時=HIGH
  bool currentBtn3 = digitalRead(BTN3_PIN);
  if (currentBtn3 != prevBtn3) {
    lastDebounceTime3 = currentTime;
  }
  if ((currentTime - lastDebounceTime3) > debounceDelay) {
    if (stableBtn3 != currentBtn3) {
      // 押された瞬間（LOW→HIGH）のみtrueに設定（離してもtrueのまま）
      if (stableBtn3 == LOW && currentBtn3 == HIGH) {
        box302State[3] = true; // 旧区画4（インデックス3）= 新区画3
        Serial.print("BTN3 pressed -> box302State[旧4→新区画3] = true");
        Serial.println();
        // 状態変化検出でcc:tweakedに通知される
      }
      // 離したとき（HIGH→LOW）は状態を変更しない（releaseリクエストまで維持）
      stableBtn3 = currentBtn3;
    }
  }
  prevBtn3 = currentBtn3;

  // BTN4 (302号室: 旧区画8 = 新区画6) の処理
  // プルダウン接続: 押していない時=LOW、押している時=HIGH
  bool currentBtn4 = digitalRead(BTN4_PIN);
  if (currentBtn4 != prevBtn4) {
    lastDebounceTime4 = currentTime;
  }
  if ((currentTime - lastDebounceTime4) > debounceDelay) {
    if (stableBtn4 != currentBtn4) {
      // 押された瞬間（LOW→HIGH）のみtrueに設定（離してもtrueのまま）
      if (stableBtn4 == LOW && currentBtn4 == HIGH) {
        box302State[7] = true; // 旧区画8（インデックス7）= 新区画6
        Serial.print("BTN4 pressed -> box302State[旧8→新区画6] = true");
        Serial.println();
        // 状態変化検出でcc:tweakedに通知される
      }
      // 離したとき（HIGH→LOW）は状態を変更しない（releaseリクエストまで維持）
      stableBtn4 = currentBtn4;
    }
  }
  prevBtn4 = currentBtn4;
  
  // 状態変化検出: 302号室の赤色変化を検出してcc:tweakedに送信
  for (int i = 0; i < 16; i++) {
    if (box302State[i] != prevBox302State[i]) {
      if (box302State[i]) {
        // 赤色に変化した
        // 旧区画番号を新区画番号に変換（302号室のマッピング）
        int newBox = ROOM302_OLD_TO_NEW[i + 1];
        if (newBox > 0 && isCCTweakedConfigured()) {
          sendToCCTweaked("302", newBox, "set");
        }
      } else {
        // 赤色が解除された
        int newBox = ROOM302_OLD_TO_NEW[i + 1];
        if (newBox > 0 && isCCTweakedConfigured()) {
          sendToCCTweaked("302", newBox, "clear");
        }
      }
      prevBox302State[i] = box302State[i];
    }
  }

  WiFiClient client = server.available();

  if (client) {
    Serial.println("new client");
    
    // --- シンプルな HTTP パーサ ---
    String requestLine = "";
    String currentLine = "";
    String body = "";
    bool isFirstLine = true;
    bool headerEnded = false;

    // タイムアウト対策
    unsigned long startTime = millis();

    while (client.connected() && (millis() - startTime) < 5000) {
      if (client.available()) {
        char c = client.read();

        if (!headerEnded) {
          // ヘッダー行の処理（\n 区切り）
          if (c == '\n') {
            // 1行終わり
            if (currentLine.length() == 0) {
              // 空行 = ヘッダー終わり
              headerEnded = true;
            } else {
              if (isFirstLine) {
                requestLine = currentLine;
                isFirstLine = false;
              }
              currentLine = "";
            }
          } else if (c != '\r') {
            currentLine += c;
          }
        } else {
          // ヘッダー終わり以降はボディとして全部読み込む
          body += c;
        }
      } else {
        // これ以上届くデータがなさそうなら抜ける
        if (headerEnded) break;
      }
    }

    requestLine.trim();
    Serial.print("Request Line: ");
    Serial.println(requestLine);

    // --- メソッド判定 ---
    bool isPost    = requestLine.startsWith("POST ");
    bool isGet     = requestLine.startsWith("GET ");
    bool isOptions = requestLine.startsWith("OPTIONS ");

    if (isOptions) {
      // --- CORS プリフライト用のレスポンス ---
      client.println("HTTP/1.1 204 No Content");
      client.println("Access-Control-Allow-Origin: *");
      client.println("Access-Control-Allow-Methods: GET, POST, OPTIONS");
      client.println("Access-Control-Allow-Headers: Content-Type");
      client.println("Connection: close");
      client.println();
    } else if (isPost) {
      // Cloudflare Tunnel 経由で Nuxt から来る JSON を想定
      Serial.print("POST body: ");
      Serial.println(body);

      // JSON パース: {"room": "203", "box": 3, "action": "set"} または {"productNumber": 3} (後方互換)
      
      // 後方互換性: productNumber の処理
      int idxProduct = body.indexOf("\"productNumber\"");
      if (idxProduct != -1) {
        int idxColon = body.indexOf(":", idxProduct);
        if (idxColon != -1) {
          int idxEnd = body.indexOf("}", idxColon);
          if (idxEnd == -1) idxEnd = body.length();
          String numStr = body.substring(idxColon + 1, idxEnd);
          numStr.trim();
          int num = numStr.toInt();

          if (num >= 1 && num <= 16) {
            box302State[num - 1] = true; // インデックスは0-15
            Serial.print("productNumber: box302State[");
            Serial.print(num - 1);
            Serial.println("] = true");
          }
        }
      }

      // 新しい形式: {"room": "302", "box": 3, "action": "set"} または {"room": "302", "action": "clear"}
      int idxRoom = body.indexOf("\"room\"");
      if (idxRoom != -1) {
        // room の値を取得
        int idxColon = body.indexOf(":", idxRoom);
        int idxComma = body.indexOf(",", idxColon);
        int idxEnd = body.indexOf("}", idxColon);
        if (idxComma != -1 && idxComma < idxEnd) idxEnd = idxComma;
        String roomStr = body.substring(idxColon + 1, idxEnd);
        roomStr.trim();
        roomStr.replace("\"", "");
        
        // box の値を取得（オプション）
        int idxBox = body.indexOf("\"box\"");
        int boxNum = -1;
        if (idxBox != -1) {
          int idxBoxColon = body.indexOf(":", idxBox);
          int idxBoxComma = body.indexOf(",", idxBoxColon);
          int idxBoxEnd = body.indexOf("}", idxBoxColon);
          if (idxBoxComma != -1 && idxBoxComma < idxBoxEnd) idxBoxEnd = idxBoxComma;
          String boxStr = body.substring(idxBoxColon + 1, idxBoxEnd);
          boxStr.trim();
          boxNum = boxStr.toInt();
        }

        // action の値を取得
        int idxAction = body.indexOf("\"action\"");
        String actionStr = "";
        if (idxAction != -1) {
          int idxActionColon = body.indexOf(":", idxAction);
          int idxActionComma = body.indexOf(",", idxActionColon);
          int idxActionEnd = body.indexOf("}", idxActionColon);
          if (idxActionComma != -1 && idxActionComma < idxActionEnd) idxActionEnd = idxActionComma;
          actionStr = body.substring(idxActionColon + 1, idxActionEnd);
          actionStr.trim();
          actionStr.replace("\"", "");
        }

        // 処理実行
        if (roomStr == "302") {
          // 新区画番号を旧区画番号に変換
          int oldBox = -1;
          for (int i = 1; i <= 16; i++) {
            if (ROOM302_OLD_TO_NEW[i] == boxNum) {
              oldBox = i;
              break;
            }
          }
          
          if (actionStr == "clear") {
            // 全解除
            if (oldBox >= 1 && oldBox <= 16) {
              box302State[oldBox - 1] = false;
              Serial.print("Clear box302State[旧");
              Serial.print(oldBox);
              Serial.print("→新区画");
              Serial.print(boxNum);
              Serial.println("] = false");
              
              // 状態が変化したので、cc:tweakedに通知
              if (isCCTweakedConfigured()) {
                sendToCCTweaked("302", boxNum, "clear");
              }
            } else if (boxNum == -1) {
              // box が指定されていない場合は全解除
              for (int i = 0; i < 16; i++) {
                box302State[i] = false;
              }
              Serial.println("Clear all box302State = false");
              
              // 全解除の場合は、cc:tweakedに通知（box=nullで送信）
              if (isCCTweakedConfigured()) {
                sendToCCTweaked("302", -1, "clear"); // box=-1は全解除を示す
              }
            }
          } else if (actionStr == "set" && oldBox >= 1 && oldBox <= 16) {
            box302State[oldBox - 1] = true;
            Serial.print("Set box302State[旧");
            Serial.print(oldBox);
            Serial.print("→新区画");
            Serial.print(boxNum);
            Serial.println("] = true");
            
            // 状態が変化したので、cc:tweakedに通知
            if (isCCTweakedConfigured()) {
              sendToCCTweaked("302", boxNum, "set");
            }
          }
        } else if (roomStr == "301") {
          if (actionStr == "clear") {
            // 全解除
            if (boxNum >= 1 && boxNum <= 12) {
              box301State[boxNum] = false; // 新しい区画番号はインデックス1-12
              Serial.print("Clear box301State[区画");
              Serial.print(boxNum);
              Serial.println("] = false");
              
              // 状態が変化したので、cc:tweakedに通知
              if (isCCTweakedConfigured()) {
                sendToCCTweaked("301", boxNum, "clear");
              }
            } else {
              // box が指定されていない場合は全解除
              for (int i = 1; i <= 12; i++) {
                box301State[i] = false;
              }
              Serial.println("Clear all box301State = false");
              
              // 全解除の場合は、cc:tweakedに通知（box=nullで送信）
              if (isCCTweakedConfigured()) {
                sendToCCTweaked("301", -1, "clear"); // box=-1は全解除を示す
              }
            }
          } else if (actionStr == "set" && boxNum >= 1 && boxNum <= 12) {
            box301State[boxNum] = true; // 新しい区画番号はインデックス1-12
            Serial.print("Set box301State[区画");
            Serial.print(boxNum);
            Serial.println("] = true");
            
            // 状態が変化したので、cc:tweakedに通知
            if (isCCTweakedConfigured()) {
              sendToCCTweaked("301", boxNum, "set");
            }
          }
        }
      }

      // POST に対しては簡単なレスポンスのみ返す（JSON でも OK）
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println("Access-Control-Allow-Origin: *");
      client.println("Connection: close");
      client.println();
      client.println("{\"status\":\"ok\"}");
    } else if (isGet) {
      // --- 通常のブラウザアクセス（GET）には HTML を返す ---
      sendDynamicPage(client);
    } else {
      // それ以外のメソッドには 405 などを返してもよい
      client.println("HTTP/1.1 405 Method Not Allowed");
      client.println("Access-Control-Allow-Origin: *");
      client.println("Access-Control-Allow-Methods: GET, POST, OPTIONS");
      client.println("Access-Control-Allow-Headers: Content-Type");
      client.println("Connection: close");
      client.println();
    }

    // 接続を閉じる
    client.stop();
    Serial.println("client disconnected");
  }
}

/**
 * @brief 動的なHTMLページをクライアントに送信します
 * @param client 送信先のWiFiClient
 */
void sendDynamicPage(WiFiClient client) {
  
  // トグルスイッチの状態も状態配列に反映（リアルタイム表示用）
  // ただし、実際の状態管理は loop() 内のエッジ検出で行う

  // --- HTTPヘッダー ---
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Access-Control-Allow-Methods: GET, POST, OPTIONS");
  client.println("Access-Control-Allow-Headers: Content-Type");
  client.println("Connection: close"); // レスポンス後に接続を閉じる
  client.println(); // ヘッダー終了

  // --- HTML開始 ---
  // cc:tweaked通信用の状態情報を先頭に配置（データサイズ削減・パース処理高速化）
  client.print("<!--BOX_STATUS:");
  // 2-301室の状態（新しい区画番号1-12をカンマ区切りで: 1=赤色, 0=通常）
  client.print("301:");
  for (int i = 1; i <= 12; i++) {
    if (i > 1) client.print(",");
    client.print(box301State[i] ? "1" : "0");
  }
  // 2-302室の状態（新しい区画番号1-7をカンマ区切りで: 1=赤色, 0=通常）
  // 新区画番号1,2,3,4,5,6,7に対応する旧区画番号の状態を表示
  client.print("|302:");
  bool first302 = true;
  for (int newBox = 1; newBox <= 7; newBox++) {
    if (!first302) client.print(",");
    // 新区画番号に対応する旧区画番号を検索
    int oldBox = -1;
    for (int i = 1; i <= 16; i++) {
      if (ROOM302_OLD_TO_NEW[i] == newBox) {
        oldBox = i;
        break;
      }
    }
    if (oldBox >= 1 && oldBox <= 16) {
      client.print(box302State[oldBox - 1] ? "1" : "0");
    } else {
      client.print("0");
    }
    first302 = false;
  }
  client.println("-->");
  
  client.println("<!DOCTYPE html><html><head>");
  client.println("<title>欅祭 呼び出しリスト</title>");
  client.println("<meta charset=\"UTF-8\">");
  client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  client.println("<meta http-equiv=\"refresh\" content=\"5\">");
  
  // --- <style> タグとCSSの読み込み ---
  client.println("<style>");
  // PROGMEMからCSSをストリーム
  client.print((const __FlashStringHelper *)STYLE_CSS); 
  client.println("</style>");
  
  client.println("</head><body>");
  client.println("<h1>欅祭 呼び出しリスト</h1>");
  client.println("<div class=\"container\">");

  // --- 部屋 2-301 ---
  client.println("<div class=\"room room-301\">");
  client.println("<div class=\"room-name\">2-301</div>");
  client.println("<div class=\"grid-container\">");
  
  // 2-301室の新しい区画番号1-12を状態配列に基づいて生成
  // 区画5は旧7と旧16の間（インデックス8）に配置
  for (int newBox = 1; newBox <= 12; newBox++) {
    client.print("<div class=\"grid-item ");
    if (box301State[newBox]) {
      client.print("highlighted");
    }
    client.print("\">");
    client.print(newBox); // 新しい区画番号を表示
    client.println("</div>");
  }
  
  client.println("</div></div>"); // grid-container, room-301 終了

  // --- 部屋 2-302 ---
  client.println("<div class=\"room room-302\">");
  client.println("<div class=\"room-name\">2-302</div>");
  client.println("<div class=\"grid-container\">");

  // 2-302室の新しい区画番号1-7を状態配列に基づいて生成
  // 新区画番号1,2,3,4,5,6,7に対応する旧区画番号の状態を表示
  for (int newBox = 1; newBox <= 7; newBox++) {
    // 新区画番号に対応する旧区画番号を検索
    int oldBox = -1;
    for (int i = 1; i <= 16; i++) {
      if (ROOM302_OLD_TO_NEW[i] == newBox) {
        oldBox = i;
        break;
      }
    }
    
    client.print("<div class=\"grid-item ");
    if (oldBox >= 1 && oldBox <= 16 && box302State[oldBox - 1]) {
      client.print("highlighted");
    }
    client.print("\">");
    client.print(newBox); // 新しい区画番号を表示
    client.println("</div>");
  }

  client.println("</div></div>"); // grid-container, room-302 終了

  client.println("</div>"); // container 終了
  client.println("</body></html>");
  client.println(); // HTTPレスポンスの最後
}


// Wi-Fiステータスをシリアルモニタに出力する関数
void printWifiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);
}

/**
 * @brief cc:tweakedサーバーが設定されているかどうかを確認
 * @return true: 設定済み, false: 未設定
 */
bool isCCTweakedConfigured() {
  return cctweaked_ip[0] != 0 || cctweaked_ip[1] != 0 || 
         cctweaked_ip[2] != 0 || cctweaked_ip[3] != 0;
}

/**
 * @brief cc:tweakedサーバーにJSONリクエストを送信
 * @param room 部屋番号 ("302" または "301")
 * @param box 区画番号 (1-16、-1の場合は全解除)
 * @param action アクション ("set" または "clear")
 */
void sendToCCTweaked(String room, int box, String action) {
  if (!isCCTweakedConfigured()) {
    return;
  }

  WiFiClient client;
  
  Serial.print("Connecting to cc:tweaked server: ");
  Serial.print(cctweaked_ip);
  Serial.print(":");
  Serial.println(cctweaked_port);

  if (client.connect(cctweaked_ip, cctweaked_port)) {
    Serial.println("Connected to cc:tweaked server");
    
    // JSON ボディを構築
    String jsonBody = "{\"room\":\"" + room + "\",\"action\":\"" + action + "\"";
    if (box > 0) {
      jsonBody += ",\"box\":" + String(box);
    }
    jsonBody += "}";
    
    // HTTP POST リクエストを送信
    client.println("POST /api/box HTTP/1.1");
    client.print("Host: ");
    client.println(cctweaked_ip);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonBody.length());
    client.println("Connection: close");
    client.println();
    client.println(jsonBody);
    
    Serial.print("Sent to cc:tweaked: ");
    Serial.println(jsonBody);
    
    // レスポンスを待つ（タイムアウト: 5秒）
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(">>> Client Timeout !");
        client.stop();
        return;
      }
    }
    
    // レスポンスを読み込んで表示
    while (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    Serial.println();
    
    client.stop();
    Serial.println("Connection to cc:tweaked closed");
  } else {
    Serial.println("Connection to cc:tweaked failed");
  }
}