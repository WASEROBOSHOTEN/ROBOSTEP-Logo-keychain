/*
 * Rikoten_APMode (Main Sketch)
 * 3輪オムニ + 2サーボアーム制御
 * - Wi-Fi APモードで起動
 * - Web UI (index.html) をホスト
 * - WebSocket (WebServerManager) で UI からの指令を受信
 * - Ticker (updateControl) で 10ms ごとに制御
 * - RobotHardware でモーター/サーボを駆動
 * - OmniController で運動学を計算
 */

// --- システムライブラリ ---
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Ticker.h>

// --- カスタムクラス ---
#include "robot_params.h"     // ★ (STEP 1) パラメータ
#include "RobotHardware.h"    // ★ (STEP 2) ハードウェア制御
#include "OmniController.h"   // ★ オムニ運動学
#include "WebServerManager.h" // ★ Web/WebSocket

// --- Wi-Fi設定 (robot_params.h に移動しても良い) ---
const char* ap_ssid = "ESP_Robot_WiFi";
const char* ap_password = "1234567890";

// --- グローバル変数 (WebSocket <-> Ticker の通信用) ---
// WebServerManager.h で extern 宣言したものと対応
volatile float g_target_vx = 0.0;     // [-1.0, +1.0]
volatile float g_target_vy = 0.0;     // [-1.0, +1.0]
volatile float g_target_omega = 0.0;  // [-1.0, +1.0]
volatile float g_target_s1_vel = 0.0; // [-1.0, +1.0]
volatile float g_target_s2_vel = 0.0; // [-1.0, +1.0]
volatile float g_target_s3_vel = 0.0; // [-1.0, +1.0]
volatile float g_target_s4_vel = 0.0; // [-1.0, +1.0]

// --- グローバル変数 (制御状態保持用) ---
float g_current_s1_angle = 90.0; // サーボ1の現在角度 (初期値90度)
float g_current_s2_angle = 90.0; // サーボ2の現在角度 (初期値90度)
float g_current_s3_angle = 90.0; // S3の現在角度
float g_current_s4_angle = 90.0; // S4の現在角度

// --- ▼▼▼ ここから不足している定義を追加 ▼▼▼ ---

// WebServerManager.h/cpp から参照されるグローバル変数
volatile bool g_led_state[3] = { true, false, false };
volatile float g_servo_min[4] = { 0.0, 40.0, 0.0, 0.0 };
volatile float g_servo_max[4] = { 130.0, 180.0, 150.0, 180.0 };

// --- デバッグ用グローバル変数 ---
volatile float g_debug_power0 = 0.0;
volatile float g_debug_power1 = 0.0;
volatile float g_debug_power2 = 0.0;

// --- オブジェクトのインスタンス化 ---
WebServerManager web_server;
RobotHardware hardware;
OmniController<NUM_MOTORS> omniController(ROBOT_RADIUS, WHEEL_RADIUS, WHEEL_ANGLES_DEG);
Ticker controlTicker;

// --- 制御ループ (Tickerから10msごとに呼ばれる) ---
void updateControl() {
  
  // 1. Webからの指令値（グローバル変数）をローカル変数にコピー
  //    (割り込み中に値が変わらないようにするため)
  noInterrupts();
  float raw_vx = g_target_vx;
  float raw_vy = g_target_vy;
  float raw_omega = g_target_omega;
  float raw_s1_vel = g_target_s1_vel;
  float raw_s2_vel = g_target_s2_vel;
  float raw_s3_vel = g_target_s3_vel;
  float raw_s4_vel = g_target_s4_vel;
  interrupts();

  // float vx = raw_vx ;
  // float vy = raw_vy ;
  // float omega = raw_omega ;
  // float s1_vel = raw_s1_vel;
  // float s2_vel = raw_s2_vel;

  // --- ▼▼▼ デッドバンド ＋ スケーリング処理 ▼▼▼ ---
  float vx = 0.0, vy = 0.0, omega = 0.0, s1_vel = 0.0, s2_vel = 0.0, s3_vel = 0.0, s4_vel = 0.0;
  // vx
  if (raw_vx > UI_DEAD_ZONE) {
    vx = mapFloat(raw_vx, UI_DEAD_ZONE, 1.0, 0.0, 1.0);
  } else if (raw_vx < -UI_DEAD_ZONE) {
    vx = mapFloat(raw_vx, -UI_DEAD_ZONE, -1.0, 0.0, -1.0);
  }
  // vy
  if (raw_vy > UI_DEAD_ZONE) {
    vy = mapFloat(raw_vy, UI_DEAD_ZONE, 1.0, 0.0, 1.0);
  } else if (raw_vy < -UI_DEAD_ZONE) {
    vy = mapFloat(raw_vy, -UI_DEAD_ZONE, -1.0, 0.0, -1.0);
  }
  // omega
  if (raw_omega > UI_DEAD_ZONE) {
    omega = mapFloat(raw_omega, UI_DEAD_ZONE, 1.0, 0.0, 1.0);
  } else if (raw_omega < -UI_DEAD_ZONE) {
    omega = mapFloat(raw_omega, -UI_DEAD_ZONE, -1.0, 0.0, -1.0);
  }
  // s1_vel
  if (raw_s1_vel > UI_DEAD_ZONE) {
    s1_vel = mapFloat(raw_s1_vel, UI_DEAD_ZONE, 1.0, 0.0, 1.0);
  } else if (raw_s1_vel < -UI_DEAD_ZONE) {
    s1_vel = mapFloat(raw_s1_vel, -UI_DEAD_ZONE, -1.0, 0.0, -1.0);
  }
  // s2_vel
  if (raw_s2_vel > UI_DEAD_ZONE) {
    s2_vel = mapFloat(raw_s2_vel, UI_DEAD_ZONE, 1.0, 0.0, 1.0);
  } else if (raw_s2_vel < -UI_DEAD_ZONE) {
    s2_vel = mapFloat(raw_s2_vel, -UI_DEAD_ZONE, -1.0, 0.0, -1.0);
  }
  // s3_vel
  if (raw_s3_vel > UI_DEAD_ZONE) {
    s3_vel = mapFloat(raw_s3_vel, UI_DEAD_ZONE, 1.0, 0.0, 1.0);
  } else if (raw_s3_vel < -UI_DEAD_ZONE) {
    s3_vel = mapFloat(raw_s3_vel, -UI_DEAD_ZONE, -1.0, 0.0, -1.0);
  }
  // s4_vel
  if (raw_s4_vel > UI_DEAD_ZONE) {
    s4_vel = mapFloat(raw_s4_vel, UI_DEAD_ZONE, 1.0, 0.0, 1.0);
  } else if (raw_s4_vel < -UI_DEAD_ZONE) {
    s4_vel = mapFloat(raw_s4_vel, -UI_DEAD_ZONE, -1.0, 0.0, -1.0);
  }

  // --- ▲▲▲ 処理 終了 ▲▲▲ ---

  // 2. サーボの角度計算 (速度を積分)
  // (s1_vel が 1.0 のとき、1秒間に90度動くと仮定)
  const float MAX_SERVO_SPEED_DPS = 90.0; // [度/秒]
  const float interval_sec = CONTROL_INTERVAL_MS / 1000.0; // 0.01秒
  
  g_current_s1_angle += s1_vel * MAX_SERVO_SPEED_DPS * interval_sec;
  g_current_s2_angle += s2_vel * MAX_SERVO_SPEED_DPS * interval_sec;
  g_current_s3_angle += s3_vel * MAX_SERVO_SPEED_DPS * interval_sec;
  g_current_s4_angle += s4_vel * MAX_SERVO_SPEED_DPS * interval_sec;

  // ★ 角度をリミット (グローバル変数の制限値を使う)
  noInterrupts(); // 制限値の読み取り
  g_current_s1_angle = constrain(g_current_s1_angle, g_servo_min[0], g_servo_max[0]);
  g_current_s2_angle = constrain(g_current_s2_angle, g_servo_min[1], g_servo_max[1]);
  g_current_s3_angle = constrain(g_current_s3_angle, g_servo_min[2], g_servo_max[2]);
  g_current_s4_angle = constrain(g_current_s4_angle, g_servo_min[3], g_servo_max[3]);
  interrupts();

  // 3. サーボへ指令 (SR0 と SR1 を使用)
  hardware.setServoAngle(0, g_current_s1_angle);
  hardware.setServoAngle(1, g_current_s2_angle);
  hardware.setServoAngle(2, g_current_s3_angle);
  hardware.setServoAngle(3, g_current_s4_angle);

  // 4. LEDへ指令 ★追加
  hardware.setLED(0, g_led_state[0]);
  hardware.setLED(1, g_led_state[1]);
  hardware.setLED(2, g_led_state[2]);

  // 4. オムニホイールの運動学計算
  // Webからの [-1.0, +1.0] の値を物理単位 [m/s], [rad/s] に変換
  float target_vx_mps = vx * MAX_LINEAR_VELOCITY;
  float target_vy_mps = vy * MAX_LINEAR_VELOCITY;
  float target_omega_rads = omega * MAX_ANGULAR_VELOCITY;
  
  omniController.setTargetVelocity(target_vx_mps, target_vy_mps, target_omega_rads);
  omniController.calculateWheelSpeeds(); // 各ホイールの目標 [rad/s] を計算

  // 5. モーターへ指令
  // [rad/s] を power [-1.0, +1.0] に正規化する
  
  // 理論上の最大車輪角速度 [rad/s] を計算
  float max_wheel_rad_s = (MAX_LINEAR_VELOCITY + MAX_ANGULAR_VELOCITY * ROBOT_RADIUS) / WHEEL_RADIUS;
  
  // max_wheel_rad_s が0にならないように保護
  if (max_wheel_rad_s == 0) max_wheel_rad_s = 1.0; 

  float power0 = omniController.getWheelSpeed(0) / max_wheel_rad_s;
  float power1 = omniController.getWheelSpeed(1) / max_wheel_rad_s;
  float power2 = omniController.getWheelSpeed(2) / max_wheel_rad_s;

  if (fabs(power0) < UI_DEAD_ZONE) { power0 = 0.0; }
  if (fabs(power1) < UI_DEAD_ZONE) { power1 = 0.0; }
  if (fabs(power2) < UI_DEAD_ZONE) { power2 = 0.0; }

  hardware.driveMotor(0, power0);
  hardware.driveMotor(1, power1);
  hardware.driveMotor(2, power2);
  // ▼▼▼ デバッグ用に値を追加 ▼▼▼
  noInterrupts();
  g_debug_power0 = power0;
  g_debug_power1 = power1;
  g_debug_power2 = power2;
  interrupts();
}

// --- 初期化 ---
void setup() {
  Serial.begin(115200);
  Serial.println("\n[Rikoten_APMode] Booting...");

  // 1. LittleFSの起動 (index.html, nipplejs.min.js が必要)
  if(!LittleFS.begin(true)){ 
    Serial.println("LittleFS Mount Failed.");
    return;
  }
  Serial.println("LittleFS mounted.");

  // 2. ハードウェアの初期化 (pinMode, ledcAttachChannel など)
  hardware.begin();
  Serial.println("Hardware initialized.");

  // 3. Wi-Fi APの起動
  Serial.println("\nConfiguring Access Point...");
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("AP SSID: ");
  Serial.println(ap_ssid);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP()); // 192.168.4.1

  // 4. Webサーバー (WebSocket含む) の起動
  web_server.begin();
  
  // 5. 制御ループ (Ticker) の開始
  controlTicker.attach_ms(CONTROL_INTERVAL_MS, updateControl);
  Serial.println("Control Ticker started.");
  
  Serial.println("\nSystem setup complete. Ready for operation.");
  digitalWrite(LED0, HIGH); // 起動完了LED
}

// --- メインループ (デバッグ表示・テレメトリ送信用) ---
unsigned long lastPrintTime = 0;
const long printInterval = 100; // 100ms ごとに表示/送信

void loop() {
  // ★ DNSサーバーのリクエストを処理 (キャプティブポータル実装時に追加)
  // dnsServer.processNextRequest();

  // 100msごとに実行
  if (millis() - lastPrintTime > printInterval) {
    lastPrintTime = millis();
    
    // --- ▼▼▼ 変数名を修正 ▼▼▼ ---
    // (s0, s1... が JsonObject と重複しないよう変更)
    noInterrupts();
    float local_s0_ang = g_current_s1_angle;
    float local_s1_ang = g_current_s2_angle;
    float local_s2_ang = g_current_s3_angle;
    float local_s3_ang = g_current_s4_angle;

    float local_s0_min = g_servo_min[0];
    float local_s0_max = g_servo_max[0];
    float local_s1_min = g_servo_min[1];
    float local_s1_max = g_servo_max[1];
    float local_s2_min = g_servo_min[2];
    float local_s2_max = g_servo_max[2];
    float local_s3_min = g_servo_min[3];
    float local_s3_max = g_servo_max[3];
    
    float local_p0 = g_debug_power0;
    float local_p1 = g_debug_power1;
    float local_p2 = g_debug_power2;
    float d0 = hardware.getDuty(0);
    float d1 = hardware.getDuty(1);
    float d2 = hardware.getDuty(2);
    interrupts();
    // --- ▲▲▲ 修正 終了 ▲▲▲ ---


    // --- 1. デバッグ表示 (シリアルモニタ) ---
    Serial.printf("ServoAng: [S0: %.2f, S1: %.2f] | MotorPwr: [M0: %.3f, M1: %.3f, M2: %.3f]\n",
                  local_s0_ang, local_s1_ang, local_p0, local_p1, local_p2);
    
    // --- 2. テレメトリJSONを作成 ---
    StaticJsonDocument<512> doc;
    JsonObject tele = doc.createNestedObject("telemetry");

    // --- ▼▼▼ 変数名を修正 ▼▼▼ ---
    // サーボ0
    JsonObject s0 = tele.createNestedObject("s0");
    s0["ang"] = local_s0_ang;
    s0["min"] = local_s0_min;
    s0["max"] = local_s0_max;
    // サーボ1
    JsonObject s1 = tele.createNestedObject("s1");
    s1["ang"] = local_s1_ang;
    s1["min"] = local_s1_min;
    s1["max"] = local_s1_max;
    // サーボ2
    JsonObject s2 = tele.createNestedObject("s2");
    s2["ang"] = local_s2_ang;
    s2["min"] = local_s2_min;
    s2["max"] = local_s2_max;
    // サーボ3
    JsonObject s3 = tele.createNestedObject("s3");
    s3["ang"] = local_s3_ang;
    s3["min"] = local_s3_min;
    s3["max"] = local_s3_max;
    
    String telemetry_json;
    serializeJson(doc, telemetry_json);
    // --- ▲▲▲ 修正 終了 ▲▲▲ ---
    
    // 3. 全クライアントに送信
    web_server.sendTelemetry(telemetry_json);
  }
}



