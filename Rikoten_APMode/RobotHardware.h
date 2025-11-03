#pragma once // ファイルが複数回includeされるのを防ぐ

#include <Arduino.h>
#include "robot_params.h" // STEP 1 で作成したパラメータを読み込む

class RobotHardware {
public:
    // コンストラクタ (何もしなくて良い)
    RobotHardware();

    // setup()から呼ばれる初期化関数
    void begin();

    // モーターを駆動する関数
    // motor_index: 0, 1, 2 (M0, M1, M2)
    // power: -1.0 (逆転最大) 〜 +1.0 (正転最大)
    void driveMotor(int motor_index, float power);

    // サーボの角度を設定する関数
    // servo_index: 0, 1, 2, 3 (SR0, SR1, SR2, SR3)
    // angle: 0.0 〜 180.0
    void setServoAngle(int servo_index, float angle);
    void setLED(int led_index, bool state);
    float getDuty(int motor_index);

private:
    // 内部でのみ使うヘルパー関数
    int duty2Pulse(float duty, int resolution);
    int servoAngle(float angle, int resolution);

    // PWMチャンネルをピンと対応付けるための配列
    // (M0, M1, M2, M3, SR0, SR1, SR2, SR3)
    const int pwm_channels[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    float duty[4] = {};
};