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
// 2-203 のどのボックスをハイライトするか (1〜16)。0 は「どれもハイライトしない」
int selectedBox203 = 0;

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

      // 非常にシンプルな JSON パース: {"productNumber": 3}
      int idx = body.indexOf("\"productNumber\"");
      if (idx != -1) {
        int idxColon = body.indexOf(":", idx);
        if (idxColon != -1) {
          int idxEnd = body.indexOf("}", idxColon);
          if (idxEnd == -1) idxEnd = body.length();
          String numStr = body.substring(idxColon + 1, idxEnd);
          numStr.trim();
          int num = numStr.toInt();

          if (num >= 1 && num <= 16) {
            selectedBox203 = num;
          } else {
            selectedBox203 = 0; // 範囲外はリセット
          }

          Serial.print("selectedBox203 = ");
          Serial.println(selectedBox203);
        }
      }

      // POST に対しては簡単なレスポンスのみ返す（JSON でも OK）
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println("Access-Control-Allow-Origin: *");
      client.println("Connection: close");
      client.println();
      client.print("{\"status\":\"ok\",\"selectedBox203\":");
      client.print(selectedBox203);
      client.println("}");
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
  
  // ★ 4つのボタンの状態を読み取る (押されている = LOW)
  bool isBtn1_Pressed = (digitalRead(BTN1_PIN) == LOW);
  bool isBtn2_Pressed = (digitalRead(BTN2_PIN) == LOW);
  bool isBtn3_Pressed = (digitalRead(BTN3_PIN) == LOW);
  bool isBtn4_Pressed = (digitalRead(BTN4_PIN) == LOW);

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
  
  // HTMLの順番 (nth-child) に合わせて動的にクラスを生成
  // (1)
  client.println("<div class=\"grid-item\"></div>");
  // (2)
  client.println("<div class=\"grid-item\"></div>");
  // (3) - BTN1
  client.print("<div class=\"grid-item ");
  if (isBtn1_Pressed) { client.print("highlighted"); }
  client.println("\"></div>");
  // (4)
  client.println("<div class=\"grid-item\"></div>");
  // (5)
  client.println("<div class=\"grid-item\"></div>");
  // (6)
  client.println("<div class=\"grid-item\"></div>");
  // (7)
  client.println("<div class=\"grid-item\"></div>");
  // (8)
  client.println("<div class=\"grid-item\"></div>");
  // (9)
  client.println("<div class=\"grid-item\"></div>");
  // (10)
  client.println("<div class=\"grid-item\"></div>");
  // (11)
  client.println("<div class=\"grid-item\"></div>");
  // (12)
  client.println("<div class=\"grid-item\"></div>");
  // (13) - BTN2
  client.print("<div class=\"grid-item ");
  if (isBtn2_Pressed) { client.print("highlighted"); }
  client.println("\"></div>");
  // (14)
  client.println("<div class=\"grid-item\"></div>");
  // (15)
  client.println("<div class=\"grid-item\"></div>");
  // (16)
  client.println("<div class=\"grid-item\"></div>");
  
  client.println("</div></div>"); // grid-container, room-204 終了

  // --- 部屋 2-203 ---
  client.println("<div class=\"room room-203\">");
  client.println("<div class=\"room-name\">2-203</div>");
  client.println("<div class=\"grid-container\">");

  // ★ 2-203 は Cloudflare Tunnel 経由の JSON（productNumber）で制御する
  // selectedBox203 (1〜16) が一致したマスを赤くハイライト
  for (int i = 1; i <= 16; i++) {
    client.print("<div class=\"grid-item ");
    if (selectedBox203 == i) {
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