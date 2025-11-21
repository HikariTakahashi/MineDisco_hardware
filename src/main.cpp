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
bool prevBtn301[16] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH,
                        HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
unsigned long lastDebounceTime301[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                         0, 0, 0, 0, 0, 0, 0, 0};
bool stableBtn301[16] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH,
                          HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};

// 2-302室のタクトスイッチ用のデバウンス変数
bool prevBtn3 = HIGH;
bool prevBtn4 = HIGH;
unsigned long lastDebounceTime3 = 0;
unsigned long lastDebounceTime4 = 0;
bool stableBtn3 = HIGH;
bool stableBtn4 = HIGH;

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

  // ★ ピンモードを INPUT_PULLUP に設定
  // 2-301室の16個のタクトスイッチ
  for (int i = 0; i < 16; i++) {
    pinMode(BTN301_PINS[i], INPUT_PULLUP);
  }
  // 2-302室のタクトスイッチ
  pinMode(BTN3_PIN, INPUT_PULLUP);
  pinMode(BTN4_PIN, INPUT_PULLUP);

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
  
  // 2-301室の16個のタクトスイッチを処理（インデックス0~15が区画1~16に対応）
  for (int i = 0; i < 16; i++) {
    bool currentBtn = digitalRead(BTN301_PINS[i]);
    if (currentBtn != prevBtn301[i]) {
      // 状態が変化したので、デバウンスタイマーをリセット
      lastDebounceTime301[i] = currentTime;
    }
    // デバウンス時間が経過したら、状態を確定
    if ((currentTime - lastDebounceTime301[i]) > debounceDelay) {
      if (stableBtn301[i] != currentBtn) {
        // 押された瞬間（HIGH→LOW）のみtrueに設定（離してもtrueのまま）
        if (stableBtn301[i] == HIGH && currentBtn == LOW) {
          box301State[i] = true;
          Serial.print("BTN301[");
          Serial.print(i);
          Serial.print("] (pin ");
          Serial.print(BTN301_PINS[i]);
          Serial.print(") pressed -> box301State[");
          Serial.print(i);
          Serial.println("] = true");
          
          // 状態が変化したので、cc:tweakedに通知
          if (isCCTweakedConfigured()) {
            sendToCCTweaked("301", i + 1, "set"); // boxは1-16
          }
        }
        // 離したとき（LOW→HIGH）は状態を変更しない（releaseリクエストまで維持）
        stableBtn301[i] = currentBtn;
      }
    }
    prevBtn301[i] = currentBtn;
  }

  // BTN3 (302号室: 左4 = インデックス3) の処理
  bool currentBtn3 = digitalRead(BTN3_PIN);
  if (currentBtn3 != prevBtn3) {
    lastDebounceTime3 = currentTime;
  }
  if ((currentTime - lastDebounceTime3) > debounceDelay) {
    if (stableBtn3 != currentBtn3) {
      // 押された瞬間（HIGH→LOW）のみtrueに設定（離してもtrueのまま）
      if (stableBtn3 == HIGH && currentBtn3 == LOW) {
        box302State[3] = true;
        Serial.print("BTN3 pressed -> box302State[3] = true");
        Serial.println();
      }
      // 離したとき（LOW→HIGH）は状態を変更しない（releaseリクエストまで維持）
      stableBtn3 = currentBtn3;
    }
  }
  prevBtn3 = currentBtn3;

  // BTN4 (302号室: 右2 = インデックス7) の処理
  bool currentBtn4 = digitalRead(BTN4_PIN);
  if (currentBtn4 != prevBtn4) {
    lastDebounceTime4 = currentTime;
  }
  if ((currentTime - lastDebounceTime4) > debounceDelay) {
    if (stableBtn4 != currentBtn4) {
      // 押された瞬間（HIGH→LOW）のみtrueに設定（離してもtrueのまま）
      if (stableBtn4 == HIGH && currentBtn4 == LOW) {
        box302State[7] = true;
        Serial.print("BTN4 pressed -> box302State[7] = true");
        Serial.println();
      }
      // 離したとき（LOW→HIGH）は状態を変更しない（releaseリクエストまで維持）
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
          if (actionStr == "clear") {
            // 全解除
            if (boxNum >= 1 && boxNum <= 16) {
              box302State[boxNum - 1] = false;
              Serial.print("Clear box302State[");
              Serial.print(boxNum - 1);
              Serial.println("] = false");
              
              // 状態が変化したので、cc:tweakedに通知
              if (isCCTweakedConfigured()) {
                sendToCCTweaked("302", boxNum, "clear");
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
            box302State[boxNum - 1] = true;
            Serial.print("Set box302State[");
            Serial.print(boxNum - 1);
            Serial.println("] = true");
            
            // 状態が変化したので、cc:tweakedに通知
            if (isCCTweakedConfigured()) {
              sendToCCTweaked("302", boxNum, "set");
            }
          }
        } else if (roomStr == "301") {
          if (actionStr == "clear") {
            // 全解除
            if (boxNum >= 1 && boxNum <= 16) {
              box301State[boxNum - 1] = false;
              Serial.print("Clear box301State[");
              Serial.print(boxNum - 1);
              Serial.println("] = false");
              
              // 状態が変化したので、cc:tweakedに通知
              if (isCCTweakedConfigured()) {
                sendToCCTweaked("301", boxNum, "clear");
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
            box301State[boxNum - 1] = true;
            Serial.print("Set box301State[");
            Serial.print(boxNum - 1);
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
  
  // 2-301室の16区画を状態配列に基づいて生成
  for (int i = 0; i < 16; i++) {
    client.print("<div class=\"grid-item ");
    if (box301State[i]) {
      client.print("highlighted");
    }
    client.println("\"></div>");
  }
  
  client.println("</div></div>"); // grid-container, room-301 終了

  // --- 部屋 2-302 ---
  client.println("<div class=\"room room-302\">");
  client.println("<div class=\"room-name\">2-302</div>");
  client.println("<div class=\"grid-container\">");

  // 2-302室の16区画を状態配列に基づいて生成
  for (int i = 0; i < 16; i++) {
    client.print("<div class=\"grid-item ");
    if (box302State[i]) {
      client.print("highlighted");
    }
    client.println("\"></div>");
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