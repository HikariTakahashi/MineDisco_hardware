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
// 2-301室の16区画に対応するタクトスイッチ（2~13ピンとA0~A3ピン）
// インデックス0~15がそれぞれ区画1~16に対応
const int BTN301_PINS[16] = {
  2,  3,  4,  5,  6,  7,  8,  9,   // ピン 2~9
  10, 11, 12, 13,                  // ピン 10~13
  A0, A1, A2, A3                   // アナログピン A0~A3
};

// --- ★ 新しい区画番号マッピングテーブル ---
// 新しい区画番号（1-16）から古いインデックス（0-15）へのマッピング
// -1は「なし」（区画が存在しないことを示す）
// 部屋301のマッピング
const int ROOM301_NEW_TO_OLD[17] = {
  -1,  // 0番は使用しない
  11,  // 新しい区画1 → 古いインデックス11 (元の区画12)
  10,  // 新しい区画2 → 古いインデックス10 (元の区画11)
  -1,  // 新しい区画3 → なし
  -1,  // 新しい区画4 → なし
  5,   // 新しい区画5 → 古いインデックス5 (元の区画6)
  -1,  // 新しい区画6 → なし
  -1,  // 新しい区画7 → なし
  9,   // 新しい区画8 → 古いインデックス9 (元の区画10)
  8,   // 新しい区画9 → 古いインデックス8 (元の区画9)
  7,   // 新しい区画10 → 古いインデックス7 (元の区画8)
  0,   // 新しい区画11 → 古いインデックス0 (元の区画1)
  1,   // 新しい区画12 → 古いインデックス1 (元の区画2)
  -1,  // 新しい区画13 → なし
  2,   // 新しい区画14 → 古いインデックス2 (元の区画3)
  -1,  // 新しい区画15 → なし
  3    // 新しい区画16 → 古いインデックス3 (元の区画4)
};

// 部屋302のマッピング
const int ROOM302_NEW_TO_OLD[17] = {
  -1,  // 0番は使用しない
  0,   // 新しい区画1 → 古いインデックス0 (元の区画1)
  1,   // 新しい区画2 → 古いインデックス1 (元の区画2)
  -1,  // 新しい区画3 → なし
  2,   // 新しい区画4 → 古いインデックス2 (元の区画3)
  -1,  // 新しい区画5 → なし
  -1,  // 新しい区画6 → なし
  6,   // 新しい区画7 → 古いインデックス6 (元の区画7)
  7,   // 新しい区画8 → 古いインデックス7 (元の区画8)
  -1,  // 新しい区画9 → なし
  9,   // 新しい区画10 → 古いインデックス9 (元の区画10)
  -1,  // 新しい区画11 → なし
  -1,  // 新しい区画12 → なし
  -1,  // 新しい区画13 → なし
  13,  // 新しい区画14 → 古いインデックス13 (元の区画14)
  -1,  // 新しい区画15 → なし
  -1   // 新しい区画16 → なし
};

// --- ★ 新しい区画番号からピン番号へのマッピング（部屋301のボタンロジック用） ---
// 新しい区画番号（1-16）からピン番号へのマッピング
// -1は「なし」（ボタンが存在しないことを示す）
const int ROOM301_NEW_TO_PIN[17] = {
  -1,  // 0番は使用しない
  2,   // 新しい区画1 → ピン2
  3,   // 新しい区画2 → ピン3
  -1,  // 新しい区画3 → なし
  5,   // 新しい区画4 → ピン5
  6,   // 新しい区画5 → ピン6
  7,   // 新しい区画6 → ピン7
  -1,  // 新しい区画7 → なし
  9,   // 新しい区画8 → ピン9
  -1,  // 新しい区画9 → なし
  11,  // 新しい区画10 → ピン11
  -1,  // 新しい区画11 → なし
  -1,  // 新しい区画12 → なし
  -1,  // 新しい区画13 → なし
  -1,  // 新しい区画14 → なし
  -1,  // 新しい区画15 → なし
  -1   // 新しい区画16 → なし
};

// 2-302室のタクトスイッチ（既存の設定を維持、必要に応じて拡張可能）
const int BTN3_PIN = 4; // 302号室: 左4
const int BTN4_PIN = 5; // 302号室: 右2

// --- リモート操作用の状態 ---
// 2-302室の16区画の状態（0=false: 非ハイライト, 1=true: ハイライト）
bool box302State[16] = {false, false, false, false, false, false, false, false,
                         false, false, false, false, false, false, false, false};

// 2-301室の16区画の状態（0=false: 非ハイライト, 1=true: ハイライト）
bool box301State[16] = {false, false, false, false, false, false, false, false,
                         false, false, false, false, false, false, false, false};

// 2-301室のタクトスイッチ用のデバウンス変数
// プルダウン接続: 押していない時=LOW、押している時=HIGH
bool prevBtn301[16] = {LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW,
                        LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW};
unsigned long lastDebounceTime301[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                         0, 0, 0, 0, 0, 0, 0, 0};
bool stableBtn301[16] = {LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW,
                          LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW};

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
bool prevBox301State[16] = {false, false, false, false, false, false, false, false,
                             false, false, false, false, false, false, false, false};

// --- 関数プロトタイプ ---
void printWifiStatus();
void sendDynamicPage(WiFiClient client);
void sendToCCTweaked(String room, int box, String action);
bool isCCTweakedConfigured();

void setup() {
  Serial.begin(9600);
  pinMode(led, OUTPUT);

  // ★ ピンモードを INPUT に設定（プルダウン接続: 押していない時=LOW、押している時=HIGH）
  // 2-301室の16個のタクトスイッチ
  for (int i = 0; i < 16; i++) {
    pinMode(BTN301_PINS[i], INPUT);
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
  
  // 2-301室のタクトスイッチを処理（新しい区画番号→ピン番号のマッピングを使用）
  // プルダウン接続: 押していない時=LOW、押している時=HIGH
  for (int newBox = 1; newBox <= 16; newBox++) {
    int pin = ROOM301_NEW_TO_PIN[newBox];
    if (pin == -1) continue; // ボタンが存在しない区画はスキップ
    
    // ピン番号からインデックスを取得
    int oldIdx = -1;
    for (int i = 0; i < 16; i++) {
      if (BTN301_PINS[i] == pin) {
        oldIdx = i;
        break;
      }
    }
    if (oldIdx == -1) continue; // ピンが見つからない場合はスキップ
    
    bool currentBtn = digitalRead(pin);
    if (currentBtn != prevBtn301[oldIdx]) {
      // 状態が変化したので、デバウンスタイマーをリセット
      lastDebounceTime301[oldIdx] = currentTime;
    }
    // デバウンス時間が経過したら、状態を確定
    if ((currentTime - lastDebounceTime301[oldIdx]) > debounceDelay) {
      if (stableBtn301[oldIdx] != currentBtn) {
        // 押された瞬間（LOW→HIGH）のみtrueに設定（離してもtrueのまま）
        if (stableBtn301[oldIdx] == LOW && currentBtn == HIGH) {
          box301State[oldIdx] = true;
          Serial.print("BTN301[新区画");
          Serial.print(newBox);
          Serial.print("] (pin ");
          Serial.print(pin);
          Serial.print(") pressed -> box301State[");
          Serial.print(oldIdx);
          Serial.println("] = true");
          
          // 状態が変化したので、cc:tweakedに通知（新しい区画番号を使用）
          if (isCCTweakedConfigured()) {
            sendToCCTweaked("301", newBox, "set");
          }
        }
        // 離したとき（HIGH→LOW）は状態を変更しない（releaseリクエストまで維持）
        stableBtn301[oldIdx] = currentBtn;
      }
    }
    prevBtn301[oldIdx] = currentBtn;
  }

  // BTN3 (302号室: 新しい区画4) の処理
  // プルダウン接続: 押していない時=LOW、押している時=HIGH
  bool currentBtn3 = digitalRead(BTN3_PIN);
  if (currentBtn3 != prevBtn3) {
    lastDebounceTime3 = currentTime;
  }
  if ((currentTime - lastDebounceTime3) > debounceDelay) {
    if (stableBtn3 != currentBtn3) {
      // 押された瞬間（LOW→HIGH）のみtrueに設定（離してもtrueのまま）
      if (stableBtn3 == LOW && currentBtn3 == HIGH) {
        int newBox = 4; // 新しい区画4
        int oldIdx = ROOM302_NEW_TO_OLD[newBox]; // 古いインデックス2
        if (oldIdx != -1) {
          box302State[oldIdx] = true;
          Serial.print("BTN3 pressed -> box302State[新区画");
          Serial.print(newBox);
          Serial.print(" -> 古いインデックス");
          Serial.print(oldIdx);
          Serial.println("] = true");
          
          // 状態が変化したので、cc:tweakedに通知（新しい区画番号を使用）
          if (isCCTweakedConfigured()) {
            sendToCCTweaked("302", newBox, "set");
          }
        }
      }
      // 離したとき（HIGH→LOW）は状態を変更しない（releaseリクエストまで維持）
      stableBtn3 = currentBtn3;
    }
  }
  prevBtn3 = currentBtn3;

  // BTN4 (302号室: 新しい区画8) の処理
  // プルダウン接続: 押していない時=LOW、押している時=HIGH
  bool currentBtn4 = digitalRead(BTN4_PIN);
  if (currentBtn4 != prevBtn4) {
    lastDebounceTime4 = currentTime;
  }
  if ((currentTime - lastDebounceTime4) > debounceDelay) {
    if (stableBtn4 != currentBtn4) {
      // 押された瞬間（LOW→HIGH）のみtrueに設定（離してもtrueのまま）
      if (stableBtn4 == LOW && currentBtn4 == HIGH) {
        int newBox = 8; // 新しい区画8
        int oldIdx = ROOM302_NEW_TO_OLD[newBox]; // 古いインデックス7
        if (oldIdx != -1) {
          box302State[oldIdx] = true;
          Serial.print("BTN4 pressed -> box302State[新区画");
          Serial.print(newBox);
          Serial.print(" -> 古いインデックス");
          Serial.print(oldIdx);
          Serial.println("] = true");
          
          // 状態が変化したので、cc:tweakedに通知（新しい区画番号を使用）
          if (isCCTweakedConfigured()) {
            sendToCCTweaked("302", newBox, "set");
          }
        }
      }
      // 離したとき（HIGH→LOW）は状態を変更しない（releaseリクエストまで維持）
      stableBtn4 = currentBtn4;
    }
  }
  prevBtn4 = currentBtn4;

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
      
      // 後方互換性: productNumber の処理（部屋302として処理）
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
            // 新しい区画番号を古いインデックスに変換
            int oldIdx = ROOM302_NEW_TO_OLD[num];
            if (oldIdx != -1) {
              box302State[oldIdx] = true;
              Serial.print("productNumber: box302State[新区画");
              Serial.print(num);
              Serial.print(" -> 古いインデックス");
              Serial.print(oldIdx);
              Serial.println("] = true");
              
              // 状態が変化したので、cc:tweakedに通知（新しい区画番号を使用）
              if (isCCTweakedConfigured()) {
                sendToCCTweaked("302", num, "set");
              }
            }
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
          if (actionStr == "clear") {
            // 全解除
            if (boxNum >= 1 && boxNum <= 16) {
              // 新しい区画番号を古いインデックスに変換
              int oldIdx = ROOM302_NEW_TO_OLD[boxNum];
              if (oldIdx != -1) {
                box302State[oldIdx] = false;
                Serial.print("Clear box302State[新区画");
                Serial.print(boxNum);
                Serial.print(" -> 古いインデックス");
                Serial.print(oldIdx);
                Serial.println("] = false");
                
                // 状態が変化したので、cc:tweakedに通知（新しい区画番号を使用）
                if (isCCTweakedConfigured()) {
                  sendToCCTweaked("302", boxNum, "clear");
                }
              }
            } else {
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
          } else if (actionStr == "set" && boxNum >= 1 && boxNum <= 16) {
            // 新しい区画番号を古いインデックスに変換
            int oldIdx = ROOM302_NEW_TO_OLD[boxNum];
            if (oldIdx != -1) {
              box302State[oldIdx] = true;
              Serial.print("Set box302State[新区画");
              Serial.print(boxNum);
              Serial.print(" -> 古いインデックス");
              Serial.print(oldIdx);
              Serial.println("] = true");
              
              // 状態が変化したので、cc:tweakedに通知（新しい区画番号を使用）
              if (isCCTweakedConfigured()) {
                sendToCCTweaked("302", boxNum, "set");
              }
            }
          }
        } else if (roomStr == "301") {
          if (actionStr == "clear") {
            // 全解除
            if (boxNum >= 1 && boxNum <= 16) {
              // 新しい区画番号を古いインデックスに変換
              int oldIdx = ROOM301_NEW_TO_OLD[boxNum];
              if (oldIdx != -1) {
                box301State[oldIdx] = false;
                Serial.print("Clear box301State[新区画");
                Serial.print(boxNum);
                Serial.print(" -> 古いインデックス");
                Serial.print(oldIdx);
                Serial.println("] = false");
                
                // 状態が変化したので、cc:tweakedに通知（新しい区画番号を使用）
                if (isCCTweakedConfigured()) {
                  sendToCCTweaked("301", boxNum, "clear");
                }
              }
            } else {
              // box が指定されていない場合は全解除
              for (int i = 0; i < 16; i++) {
                box301State[i] = false;
              }
              Serial.println("Clear all box301State = false");
              
              // 全解除の場合は、cc:tweakedに通知（box=nullで送信）
              if (isCCTweakedConfigured()) {
                sendToCCTweaked("301", -1, "clear"); // box=-1は全解除を示す
              }
            }
          } else if (actionStr == "set" && boxNum >= 1 && boxNum <= 16) {
            // 新しい区画番号を古いインデックスに変換
            int oldIdx = ROOM301_NEW_TO_OLD[boxNum];
            if (oldIdx != -1) {
              box301State[oldIdx] = true;
              Serial.print("Set box301State[新区画");
              Serial.print(boxNum);
              Serial.print(" -> 古いインデックス");
              Serial.print(oldIdx);
              Serial.println("] = true");
              
              // 状態が変化したので、cc:tweakedに通知（新しい区画番号を使用）
              if (isCCTweakedConfigured()) {
                sendToCCTweaked("301", boxNum, "set");
              }
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
  // 2-301室の状態（16区画をカンマ区切りで: 1=赤色, 0=通常）
  client.print("301:");
  for (int i = 0; i < 16; i++) {
    if (i > 0) client.print(",");
    client.print(box301State[i] ? "1" : "0");
  }
  // 2-302室の状態（16区画をカンマ区切りで: 1=赤色, 0=通常）
  client.print("|302:");
  for (int i = 0; i < 16; i++) {
    if (i > 0) client.print(",");
    client.print(box302State[i] ? "1" : "0");
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
  
  // 左列（Col 1、上から下）: 1, 2, ▫️, 8, 12, ▫️, 11
  int box301Left[] = {1, 2, -1, 8, 12, -1, 11};
  for (int i = 0; i < 7; i++) {
    int newBox = box301Left[i];
    client.print("<div class=\"grid-item ");
    if (newBox != -1) {
      int oldIdx = ROOM301_NEW_TO_OLD[newBox];
      if (oldIdx != -1 && box301State[oldIdx]) {
        client.print("highlighted");
      }
    }
    client.print("\">");
    if (newBox != -1) {
      client.print(newBox);
    }
    client.println("</div>");
  }
  
  // 中央列（Col 2、上から下）: ◾️(なし), 10, ◾️(なし), 9, ◾️(なし), ◾️(なし), ◾️(なし)
  int box301Center[] = {-1, 10, -1, 9, -1, -1, -1};
  for (int i = 0; i < 7; i++) {
    int newBox = box301Center[i];
    if (newBox != -1) {
      client.print("<div class=\"grid-item ");
      int oldIdx = ROOM301_NEW_TO_OLD[newBox];
      if (oldIdx != -1 && box301State[oldIdx]) {
        client.print("highlighted");
      }
      client.print("\">");
      client.print(newBox);
      client.println("</div>");
    }
  }
  
  // 右列（Col 3、上から下）: 3, 4, ▫️, 5, ▫️, ▫️, 6
  int box301Right[] = {3, 4, -1, 5, -1, -1, 6};
  for (int i = 0; i < 7; i++) {
    int newBox = box301Right[i];
    client.print("<div class=\"grid-item ");
    if (newBox != -1) {
      int oldIdx = ROOM301_NEW_TO_OLD[newBox];
      if (oldIdx != -1 && box301State[oldIdx]) {
        client.print("highlighted");
      }
    }
    client.print("\">");
    if (newBox != -1) {
      client.print(newBox);
    }
    client.println("</div>");
  }
  
  client.println("</div></div>"); // grid-container, room-301 終了

  // --- 部屋 2-302 ---
  client.println("<div class=\"room room-302\">");
  client.println("<div class=\"room-name\">2-302</div>");
  client.println("<div class=\"grid-container\">");

  // 左列（Col 1、上から下）: 1, 2, ▫️, 3, 空白, ▫️, 空白
  int box302Col1[] = {1, 2, -1, 3, -2, -1, -2};
  for (int i = 0; i < 7; i++) {
    int newBox = box302Col1[i];
    client.print("<div class=\"grid-item ");
    if (newBox > 0) { // 正の数は区画番号
      int oldIdx = ROOM302_NEW_TO_OLD[newBox];
      if (oldIdx != -1 && box302State[oldIdx]) {
        client.print("highlighted");
      }
    }
    client.print("\">");
    if (newBox > 0) {
      client.print(newBox);
    }
    // -1は空のdiv（▫️）、-2は空白（表示なし）
    client.println("</div>");
  }
  
  // Col 2（上から下）: なし, なし, なし, なし, なし, なし, 空白
  // Row 7のみ空のdivを表示（空白）
  client.print("<div class=\"grid-item ");
  client.print("\">");
  client.println("</div>"); // Row 7: 空白
  
  // 中央列（Col 3、上から下）: 空白, なし, ▫️, なし, なし, なし, 4
  // Row 1は空白（空のdiv）、Row 3は▫️（空のdiv）、Row 7は4
  client.print("<div class=\"grid-item ");
  client.print("\">");
  client.println("</div>"); // Row 1: 空白
  client.print("<div class=\"grid-item ");
  client.print("\">");
  client.println("</div>"); // Row 3: ▫️
  // Row 7: 4
  int box302Col3Row7 = 4;
  client.print("<div class=\"grid-item ");
  int oldIdxCol3Row7 = ROOM302_NEW_TO_OLD[box302Col3Row7];
  if (oldIdxCol3Row7 != -1 && box302State[oldIdxCol3Row7]) {
    client.print("highlighted");
  }
  client.print("\">");
  client.print(box302Col3Row7);
  client.println("</div>");
  
  // Col 4（上から下）: 空白, なし, なし, なし, なし, なし, ▫️
  // Row 1は空白（空のdiv）、Row 7は▫️（空のdiv）
  client.print("<div class=\"grid-item ");
  client.print("\">");
  client.println("</div>"); // Row 1: 空白
  client.print("<div class=\"grid-item ");
  client.print("\">");
  client.println("</div>"); // Row 7: ▫️
  
  // 右列（Col 5、上から下）: 5, 6, ▫️, 7, ▫️, ▫️, 空白
  int box302Col5[] = {5, 6, -1, 7, -1, -1, -2};
  for (int i = 0; i < 7; i++) {
    int newBox = box302Col5[i];
    client.print("<div class=\"grid-item ");
    if (newBox > 0) { // 正の数は区画番号
      int oldIdx = ROOM302_NEW_TO_OLD[newBox];
      if (oldIdx != -1 && box302State[oldIdx]) {
        client.print("highlighted");
      }
    }
    client.print("\">");
    if (newBox > 0) {
      client.print(newBox);
    }
    // -1は空のdiv（▫️）、-2は空白（表示なし）
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