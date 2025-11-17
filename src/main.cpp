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
// 2-204室の16区画に対応するタクトスイッチ（2~13ピンとA0~A3ピン）
// インデックス0~15がそれぞれ区画1~16に対応
const int BTN204_PINS[16] = {
  2,  3,  4,  5,  6,  7,  8,  9,   // ピン 2~9
  10, 11, 12, 13,                  // ピン 10~13
  A0, A1, A2, A3                   // アナログピン A0~A3
};

// 2-203室のタクトスイッチ（既存の設定を維持、必要に応じて拡張可能）
const int BTN3_PIN = 4; // 203号室: 左4
const int BTN4_PIN = 5; // 203号室: 右2

// --- リモート操作用の状態 ---
// 2-203室の16区画の状態（0=false: 非ハイライト, 1=true: ハイライト）
bool box203State[16] = {false, false, false, false, false, false, false, false,
                         false, false, false, false, false, false, false, false};

// 2-204室の16区画の状態（0=false: 非ハイライト, 1=true: ハイライト）
bool box204State[16] = {false, false, false, false, false, false, false, false,
                         false, false, false, false, false, false, false, false};

// 2-204室のタクトスイッチ用のデバウンス変数
bool prevBtn204[16] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH,
                        HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
unsigned long lastDebounceTime204[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                         0, 0, 0, 0, 0, 0, 0, 0};
bool stableBtn204[16] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH,
                          HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};

// 2-203室のタクトスイッチ用のデバウンス変数
bool prevBtn3 = HIGH;
bool prevBtn4 = HIGH;
unsigned long lastDebounceTime3 = 0;
unsigned long lastDebounceTime4 = 0;
bool stableBtn3 = HIGH;
bool stableBtn4 = HIGH;

const unsigned long debounceDelay = 50; // 50ms のデバウンス時間

// --- 関数プロトタイプ ---
void printWifiStatus();
void sendDynamicPage(WiFiClient client);

void setup() {
  Serial.begin(9600);
  pinMode(led, OUTPUT);

  // ★ ピンモードを INPUT_PULLUP に設定
  // 2-204室の16個のタクトスイッチ
  for (int i = 0; i < 16; i++) {
    pinMode(BTN204_PINS[i], INPUT_PULLUP);
  }
  // 2-203室のタクトスイッチ
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

  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to Network named: ");
    Serial.println(ssid);

    status = WiFi.begin(ssid, pass);
    delay(10000);
  }
  server.begin();
  printWifiStatus();
}


void loop() {
  // --- トグルスイッチの状態をチェック（押した瞬間にtrue、releaseリクエストまで維持） ---
  unsigned long currentTime = millis();
  
  // 2-204室の16個のタクトスイッチを処理（インデックス0~15が区画1~16に対応）
  for (int i = 0; i < 16; i++) {
    bool currentBtn = digitalRead(BTN204_PINS[i]);
    if (currentBtn != prevBtn204[i]) {
      // 状態が変化したので、デバウンスタイマーをリセット
      lastDebounceTime204[i] = currentTime;
    }
    // デバウンス時間が経過したら、状態を確定
    if ((currentTime - lastDebounceTime204[i]) > debounceDelay) {
      if (stableBtn204[i] != currentBtn) {
        // 押された瞬間（HIGH→LOW）のみtrueに設定（離してもtrueのまま）
        if (stableBtn204[i] == HIGH && currentBtn == LOW) {
          box204State[i] = true;
          Serial.print("BTN204[");
          Serial.print(i);
          Serial.print("] (pin ");
          Serial.print(BTN204_PINS[i]);
          Serial.print(") pressed -> box204State[");
          Serial.print(i);
          Serial.println("] = true");
        }
        // 離したとき（LOW→HIGH）は状態を変更しない（releaseリクエストまで維持）
        stableBtn204[i] = currentBtn;
      }
    }
    prevBtn204[i] = currentBtn;
  }

  // BTN3 (203号室: 左4 = インデックス3) の処理
  bool currentBtn3 = digitalRead(BTN3_PIN);
  if (currentBtn3 != prevBtn3) {
    lastDebounceTime3 = currentTime;
  }
  if ((currentTime - lastDebounceTime3) > debounceDelay) {
    if (stableBtn3 != currentBtn3) {
      // 押された瞬間（HIGH→LOW）のみtrueに設定（離してもtrueのまま）
      if (stableBtn3 == HIGH && currentBtn3 == LOW) {
        box203State[3] = true;
        Serial.print("BTN3 pressed -> box203State[3] = true");
        Serial.println();
      }
      // 離したとき（LOW→HIGH）は状態を変更しない（releaseリクエストまで維持）
      stableBtn3 = currentBtn3;
    }
  }
  prevBtn3 = currentBtn3;

  // BTN4 (203号室: 右2 = インデックス7) の処理
  bool currentBtn4 = digitalRead(BTN4_PIN);
  if (currentBtn4 != prevBtn4) {
    lastDebounceTime4 = currentTime;
  }
  if ((currentTime - lastDebounceTime4) > debounceDelay) {
    if (stableBtn4 != currentBtn4) {
      // 押された瞬間（HIGH→LOW）のみtrueに設定（離してもtrueのまま）
      if (stableBtn4 == HIGH && currentBtn4 == LOW) {
        box203State[7] = true;
        Serial.print("BTN4 pressed -> box203State[7] = true");
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
            box203State[num - 1] = true; // インデックスは0-15
            Serial.print("productNumber: box203State[");
            Serial.print(num - 1);
            Serial.println("] = true");
          }
        }
      }

      // 新しい形式: {"room": "203", "box": 3, "action": "set"} または {"room": "203", "action": "clear"}
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
        if (roomStr == "203") {
          if (actionStr == "clear") {
            // 全解除
            if (boxNum >= 1 && boxNum <= 16) {
              box203State[boxNum - 1] = false;
              Serial.print("Clear box203State[");
              Serial.print(boxNum - 1);
              Serial.println("] = false");
            } else {
              // box が指定されていない場合は全解除
              for (int i = 0; i < 16; i++) {
                box203State[i] = false;
              }
              Serial.println("Clear all box203State = false");
            }
          } else if (actionStr == "set" && boxNum >= 1 && boxNum <= 16) {
            box203State[boxNum - 1] = true;
            Serial.print("Set box203State[");
            Serial.print(boxNum - 1);
            Serial.println("] = true");
          }
        } else if (roomStr == "204") {
          if (actionStr == "clear") {
            // 全解除
            if (boxNum >= 1 && boxNum <= 16) {
              box204State[boxNum - 1] = false;
              Serial.print("Clear box204State[");
              Serial.print(boxNum - 1);
              Serial.println("] = false");
            } else {
              // box が指定されていない場合は全解除
              for (int i = 0; i < 16; i++) {
                box204State[i] = false;
              }
              Serial.println("Clear all box204State = false");
            }
          } else if (actionStr == "set" && boxNum >= 1 && boxNum <= 16) {
            box204State[boxNum - 1] = true;
            Serial.print("Set box204State[");
            Serial.print(boxNum - 1);
            Serial.println("] = true");
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

  // --- 部屋 2-204 ---
  client.println("<div class=\"room room-204\">");
  client.println("<div class=\"room-name\">2-204</div>");
  client.println("<div class=\"grid-container\">");
  
  // 2-204室の16区画を状態配列に基づいて生成
  for (int i = 0; i < 16; i++) {
    client.print("<div class=\"grid-item ");
    if (box204State[i]) {
      client.print("highlighted");
    }
    client.println("\"></div>");
  }
  
  client.println("</div></div>"); // grid-container, room-204 終了

  // --- 部屋 2-203 ---
  client.println("<div class=\"room room-203\">");
  client.println("<div class=\"room-name\">2-203</div>");
  client.println("<div class=\"grid-container\">");

  // 2-203室の16区画を状態配列に基づいて生成
  for (int i = 0; i < 16; i++) {
    client.print("<div class=\"grid-item ");
    if (box203State[i]) {
      client.print("highlighted");
    }
    client.println("\"></div>");
  }

  client.println("</div></div>"); // grid-container, room-203 終了

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