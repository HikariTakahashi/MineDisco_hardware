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
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        // Serial.write(c); // デバッグ時以外は不要
        if (c == '\n') {
          // HTTPリクエストの終わりを検出
          if (currentLine.length() == 0) {
            
            // ★ 動的なHTMLページを送信する関数を呼び出す
            sendDynamicPage(client);
            
            break; // レスポンスを送信したのでループを抜ける
          } else {
            currentLine = ""; // 次の行の準備
          }
        } else if (c != '\r') {
          currentLine += c; // 行に文字を追加
        }
        
        // ★ LED点灯ロジックはデバッグ用に残します
        if (currentLine.endsWith("GET /H")) {
          digitalWrite(LED_BUILTIN, HIGH);
        }
        if (currentLine.endsWith("GET /L")) {
          digitalWrite(LED_BUILTIN, LOW);
        }
      }
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

  // (1)
  client.println("<div class=\"grid-item\"></div>");
  // (2)
  client.println("<div class=\"grid-item\"></div>");
  // (3)
  client.println("<div class=\"grid-item\"></div>");
  // (4) - BTN3
  client.print("<div class=\"grid-item ");
  if (isBtn3_Pressed) { client.print("highlighted"); }
  client.println("\"></div>");
  // (5)
  client.println("<div class=\"grid-item\"></div>");
  // (6)
  client.println("<div class=\"grid-item\"></div>");
  // (7)
  client.println("<div class=\"grid-item\"></div>");
  // (8) - BTN4
  client.print("<div class=\"grid-item ");
  if (isBtn4_Pressed) { client.print("highlighted"); }
  client.println("\"></div>");
  // (9)
  client.println("<div class=\"grid-item\"></div>");
  // (10)
  client.println("<div class=\"grid-item\"></div>");
  // (11)
  client.println("<div class=\"grid-item\"></div>");
  // (12)
  client.println("<div class=\"grid-item\"></div>");
  // (13)
  client.println("<div class=\"grid-item\"></div>");
  // (14)
  client.println("<div class=\"grid-item\"></div>");
  // (15)
  client.println("<div class=\"grid-item\"></div>");
  // (16)
  client.println("<div class=\"grid-item\"></div>");

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