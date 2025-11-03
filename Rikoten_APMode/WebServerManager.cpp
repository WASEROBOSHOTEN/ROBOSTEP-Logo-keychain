#include "WebServerManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h> // JSONのパースに必要

WebServerManager::WebServerManager()
: http_server_(80), ws_("/ws") { // HTTPは80, WebSocketは /ws
}

void WebServerManager::begin() {
  setupRoutes();
  
  // WebSocketのイベントハンドラを登録
  ws_.onEvent(std::bind(&WebServerManager::onWsEvent, this, 
              std::placeholders::_1, std::placeholders::_2, 
              std::placeholders::_3, std::placeholders::_4, 
              std::placeholders::_5, std::placeholders::_6));
              
  http_server_.addHandler(&ws_);
  http_server_.begin();
  Serial.println("HTTP server started on port 80.");
}

/**
 * @brief 接続中の全クライアントにテレメトリJSONを送信
 */
void WebServerManager::sendTelemetry(String json) {
  ws_.textAll(json);
}

/**
 * @brief HTTPのルーティング設定
 */
void WebServerManager::setupRoutes() {
  // 1. ルート ("/") へのアクセスは index.html を返す
  http_server_.on("/", HTTP_GET, [&](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });

  // 2. /nipplejs.min.js へのアクセスは同名のファイルを返す
  http_server_.on("/nipplejs.min.js", HTTP_GET, [&](AsyncWebServerRequest *request){
    request->send(LittleFS, "/nipplejs.min.js", "application/javascript");
  });

  // 3. その他の不明なリクエストは 404 Not Found を返す
  http_server_.onNotFound([&](AsyncWebServerRequest *request){
    request->send(404, "text/plain", "Not found");
  });
}

/**
 * @brief WebSocketのイベント処理 (接続/切断/データ受信)
 */
void WebServerManager::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  
  switch (type) {
    // クライアントが接続した時
    case WS_EVT_CONNECT:
      Serial.printf("[WS] Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;

    // クライアントが切断した時
    case WS_EVT_DISCONNECT:
      Serial.printf("[WS] Client #%u disconnected\n", client->id());
      // --- 安全停止: 全ての指令値を0にする ---
      noInterrupts();
      g_target_vx = 0.0;
      g_target_vy = 0.0;
      g_target_omega = 0.0;
      g_target_s1_vel = 0.0;
      g_target_s2_vel = 0.0;
      g_target_s3_vel = 0.0;
      g_target_s4_vel = 0.0;
      interrupts();
      break;
      
    // データを受信した時
    case WS_EVT_DATA:
    { // 新しいスコープ
      // JSONをパース
      StaticJsonDocument<256> doc; // 256byte確保 (設定JSONも入るように)
      DeserializationError error = deserializeJson(doc, (const char*)data, len);

      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
      }

      // --- 受信したJSONの「種類」を判別 ---
      noInterrupts(); // Tickerと競合しないよう割り込み停止

      // 1. 制御指令 (controlState) か？
      if (doc.containsKey("vx")) {
        g_target_vx = doc["vx"] | 0.0;
        g_target_vy = doc["vy"] | 0.0;
        g_target_omega = doc["omega"] | 0.0;
        g_target_s1_vel = doc["s1_vel"] | 0.0;
        g_target_s2_vel = doc["s2_vel"] | 0.0;
        g_target_s3_vel = doc["s3_vel"] | 0.0;
        g_target_s4_vel = doc["s4_vel"] | 0.0;
      }
      
      // 2. LEDトグル指令か？
      else if (doc.containsKey("toggle_led")) {
        int led_index = doc["toggle_led"] | -1;
        if (led_index >= 0 && led_index < 3) {
          g_led_state[led_index] = !g_led_state[led_index]; // 状態を反転
        }
      }

      // 3. サーボ制限設定指令か？
      else if (doc.containsKey("set_limit")) {
        JsonObject limit = doc["set_limit"];
        int servo_index = limit["servo"] | -1;
        if (servo_index >= 0 && servo_index < 4) {
          // .ino側で使うグローバル変数に値を書き込む
          g_servo_min[servo_index] = limit["min"] | 0;
          g_servo_max[servo_index] = limit["max"] | 180;
        }
      }
      
      interrupts(); // 割り込み再開
    }
    break;
      
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}