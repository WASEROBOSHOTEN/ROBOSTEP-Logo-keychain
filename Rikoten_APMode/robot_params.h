#pragma once 

//================================================
// 制御設定
//================================================
constexpr int CONTROL_INTERVAL_MS = 10; // 制御周期 [ms]

//================================================
// ハードウェアピン設定
//================================================
// ユーザLED
constexpr int LED0 = 2;
constexpr int LED1 = 4;
constexpr int LED2 = 21;
// サーボモーター用
constexpr int SR0 = 16;
constexpr int SR1 = 17;
constexpr int SR2 = 18;
constexpr int SR3 = 19;
// モータードライバ用ピン (motor0, 1, 2)
constexpr int PH0 = 32;
constexpr int EN0 = 33;
constexpr int PH1 = 25;
constexpr int EN1 = 26;
constexpr int PH2 = 27;
constexpr int EN2 = 13;
constexpr int PH3 = 23;
constexpr int EN3 = 22;
// スイッチ
constexpr int SW0 = 34;
constexpr int SW1 = 35;

//================================================
// PWM設定
//================================================
constexpr int MOTOR_PWM_FREQUENCY = 50000; // モーターのPWM周波数 [Hz]
constexpr int SERVO_PWM_FREQUENCY = 50;     // サーボのPWM周波数 [Hz]
constexpr int MOTOR_PWM_RESOLUTION = 8;     // モーターPWMの解像度 [bit] (8bit = 0-255)
constexpr int SERVO_PWM_RESOLUTION = 12;    // サーボPWMの解像度 [bit] (12bit = 0-4095)
constexpr float MAX_DUTY = 0.99;            // モーターデューティ比の最大値 (1.0未満)

//================================================
// マシン設定
//================================================
constexpr float ROBOT_RADIUS = 0.075; // ロボット中心から各ホイール中心までの距離 [m]
constexpr float WHEEL_RADIUS = 0.0325; // 車輪の半径 [m]
constexpr int NUM_MOTORS = 3;
const float WHEEL_ANGLES_DEG[NUM_MOTORS] = {270.0, 60.0, 120.0}; // 右方向(X軸正)を0度

//================================================
// 制御演算値
//================================================
constexpr float MAX_LINEAR_VELOCITY = 1.0;   // 最大並進速度 [m/s]
constexpr float MAX_ANGULAR_VELOCITY = 10.0;  // 最大角速度 [rad/s]

//================================================
// モーター特性 (デッドゾーン)
//================================================
// モーターが回転し始める最小電力 (0.0〜1.0)
// (Find_Motor_Threshold_FwdRev.ino で測定した値)
constexpr float MOTOR_MIN_POWER_FWD = 0.7; // 正転
constexpr float MOTOR_MIN_POWER_REV = 0.7; // 逆転 (絶対値)

//================================================
// UI デッドバンド
//================================================
// Web UIからの指令値がこの値の絶対値未満の場合、0として扱う
constexpr float UI_DEAD_ZONE = 0.05;

// --- ヘルパ関数 ---
inline float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}