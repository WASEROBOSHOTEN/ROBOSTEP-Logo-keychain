#pragma once

#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>

// --- .ino側で定義されるグローバル変数 ---
// これらを .cpp 側で読み書きするために extern で宣言する

// 制御指令 (UI -> ESP)
extern volatile float g_target_vx;
extern volatile float g_target_vy;
extern volatile float g_target_omega;
extern volatile float g_target_s1_vel;
extern volatile float g_target_s2_vel;
extern volatile float g_target_s3_vel;
extern volatile float g_target_s4_vel;

// LED状態 (UI <-> ESP)
extern volatile bool g_led_state[3];

// サーボ制限 (UI -> ESP)
extern volatile float g_servo_min[4];
extern volatile float g_servo_max[4];


/**
 * @brief Web/WebSocketサーバーの管理クラス
 * 役割:
 * 1. HTTPサーバーを起動し、index.html / nipplejs.min.js を提供
 * 2. WebSocketサーバーを起動
 * 3. WebSocketでUIからJSONを受け取り、グローバル変数を更新
 * 4. ESP32からUIへテレメトリJSONをブロードキャスト
 */
class WebServerManager {
public:
  // コンストラクタ
  WebServerManager();
  
  // サーバーを起動
  void begin();

  // 接続中の全クライアントにテレメトリJSONを送信
  void sendTelemetry(String json);

private:
  // HTTPサーバーのルーティング（URLの割り当て）を設定
  void setupRoutes();
  
  // WebSocketのイベント（接続/切断/データ受信）を処理
  void onWsEvent(
    AsyncWebSocket *server, 
    AsyncWebSocketClient *client, 
    AwsEventType type, 
    void *arg, 
    uint8_t *data, 
    size_t len
  );

  // サーバーのインスタンス
  AsyncWebServer http_server_;
  AsyncWebSocket ws_;
};