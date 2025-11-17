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
const int BTN1_PIN = 2; // 204号室: 左3
const int BTN2_PIN = 3; // 204号室: 右4
const int BTN3_PIN = 4; // 203号室: 左4
const int BTN4_PIN = 5; // 203号室: 右2

// --- リモート操作用の状態 ---
// 2-203室の16区画の状態（0=false: 非ハイライト, 1=true: ハイライト）
bool box203State[16] = {false, false, false, false, false, false, false, false,
                         false, false, false, false, false, false, false, false};

// 2-204室の16区画の状態（0=false: 非ハイライト, 1=true: ハイライト）
bool box204State[16] = {false, false, false, false, false, false, false, false,
                         false, false, false, false, false, false, false, false};

// トグルスイッチの前回の状態（エッジ検出用）
bool prevBtn1 = HIGH;
bool prevBtn2 = HIGH;
bool prevBtn3 = HIGH;
bool prevBtn4 = HIGH;

// --- 関数プロトタイプ ---
void printWifiStatus();
void sendDynamicPage(WiFiClient client);

void setup() {
  Serial.begin(9600);
  pinMode(led, OUTPUT);

  // ★ ピンモードを INPUT_PULLUP に設定
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
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
  // --- トグルスイッチの状態をチェック（エッジ検出） ---
  bool currentBtn1 = digitalRead(BTN1_PIN);
  bool currentBtn2 = digitalRead(BTN2_PIN);
  bool currentBtn3 = digitalRead(BTN3_PIN);
  bool currentBtn4 = digitalRead(BTN4_PIN);

  // BTN1 (204号室: 左3 = インデックス2) が押された瞬間を検出
  if (prevBtn1 == HIGH && currentBtn1 == LOW) {
    box204State[2] = !box204State[2]; // トグル
    Serial.print("BTN1 pressed. box204State[2] = ");
    Serial.println(box204State[2]);
  }
  prevBtn1 = currentBtn1;

  // BTN2 (204号室: 右4 = インデックス12) が押された瞬間を検出
  if (prevBtn2 == HIGH && currentBtn2 == LOW) {
    box204State[12] = !box204State[12]; // トグル
    Serial.print("BTN2 pressed. box204State[12] = ");
    Serial.println(box204State[12]);
  }
  prevBtn2 = currentBtn2;

  // BTN3 (203号室: 左4 = インデックス3) が押された瞬間を検出
  if (prevBtn3 == HIGH && currentBtn3 == LOW) {
    box203State[3] = !box203State[3]; // トグル
    Serial.print("BTN3 pressed. box203State[3] = ");
    Serial.println(box203State[3]);
  }
  prevBtn3 = currentBtn3;

  // BTN4 (203号室: 右2 = インデックス7) が押された瞬間を検出
  if (prevBtn4 == HIGH && currentBtn4 == LOW) {
    box203State[7] = !box203State[7]; // トグル
    Serial.print("BTN4 pressed. box203State[7] = ");
    Serial.println(box203State[7]);
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