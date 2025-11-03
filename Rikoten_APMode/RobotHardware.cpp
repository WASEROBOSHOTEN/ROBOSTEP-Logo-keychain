#include "RobotHardware.h"

RobotHardware::RobotHardware() {
  // コンストラクタ (現在は何もしない)
}

/**
 * @brief ハードウェアのピンモードとPWMチャンネルを初期化
 */
void RobotHardware::begin() {
  // --- ピンモード設定 ---
  pinMode(LED0, OUTPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);

  pinMode(PH0, OUTPUT);
  pinMode(EN0, OUTPUT);
  pinMode(PH1, OUTPUT);
  pinMode(EN1, OUTPUT);
  pinMode(PH2, OUTPUT);
  pinMode(EN2, OUTPUT);
  pinMode(PH3, OUTPUT);
  pinMode(EN3, OUTPUT);

  pinMode(SW0, INPUT);
  pinMode(SW1, INPUT);

  // --- LED初期状態 ---
  digitalWrite(LED0, LOW);
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);

  // --- モータードライバ初期状態 ---
  digitalWrite(PH0, LOW);
  digitalWrite(PH1, LOW);
  digitalWrite(PH2, LOW);
  digitalWrite(PH3, LOW);

  // --- PWMチャンネルの設定 ---
  // モーター用 (M0, M1, M2, M3)
  ledcAttachChannel(EN0, MOTOR_PWM_FREQUENCY, MOTOR_PWM_RESOLUTION, pwm_channels[0]);
  ledcAttachChannel(EN1, MOTOR_PWM_FREQUENCY, MOTOR_PWM_RESOLUTION, pwm_channels[1]);
  ledcAttachChannel(EN2, MOTOR_PWM_FREQUENCY, MOTOR_PWM_RESOLUTION, pwm_channels[2]);
  ledcAttachChannel(EN3, MOTOR_PWM_FREQUENCY, MOTOR_PWM_RESOLUTION, pwm_channels[3]);
  
  // サーボ用 (SR0, SR1, SR2, SR3)
  ledcAttachChannel(SR0, SERVO_PWM_FREQUENCY, SERVO_PWM_RESOLUTION, pwm_channels[4]);
  ledcAttachChannel(SR1, SERVO_PWM_FREQUENCY, SERVO_PWM_RESOLUTION, pwm_channels[5]);
  ledcAttachChannel(SR2, SERVO_PWM_FREQUENCY, SERVO_PWM_RESOLUTION, pwm_channels[6]);
  ledcAttachChannel(SR3, SERVO_PWM_FREQUENCY, SERVO_PWM_RESOLUTION, pwm_channels[7]);

  // 全PWM出力を0に初期化
  for (int i = 0; i < 8; i++) {
    ledcWriteChannel(pwm_channels[i], 0);
  }
}

/**
 * @brief 指定したモーターを駆動
 * @param motor_index 0, 1, 2 (M0, M1, M2)
 * @param power -1.0 (逆転) 〜 +1.0 (正転)
 */
void RobotHardware::driveMotor(int motor_index, float power) {
  // powerを -1.0 〜 1.0 の範囲に制限
  power = constrain(power, -1.0, 1.0);

  // --- ▼▼▼ モーターデッドゾーン補正 ▼▼▼ ---
  float output_power = 0.0;
  
  if (power > 0.0) {
    // 正転: 0.0〜1.0 の入力を、MOTOR_MIN_POWER_FWD〜1.0 の範囲にマッピング
    output_power = mapFloat(power, 0.0, 1.0, MOTOR_MIN_POWER_FWD, 1.0);
  
  } else if (power < 0.0) {
    // 逆転: 0.0〜-1.0 の入力を、-MOTOR_MIN_POWER_REV〜-1.0 の範囲にマッピング
    // (逆転のMIN_POWERも正の値で定義されていると仮定)
    output_power = mapFloat(power, 0.0, -1.0, -MOTOR_MIN_POWER_REV, -1.0);
  }


  // 補正後のpower値で方向とデューティを決定
  bool direction = (output_power >= 0); // true = 正転, false = 逆転
  // float duty = abs(output_power) * MAX_DUTY; // デューティ比を計算
  duty[motor_index] = abs(output_power) * MAX_DUTY;
  // int pulse = duty2Pulse(duty, MOTOR_PWM_RESOLUTION); // PWMパルス幅に変換
  int pulse = duty2Pulse(duty[motor_index], MOTOR_PWM_RESOLUTION); // PWMパルス幅に変換

  switch (motor_index) {
    case 0:
      digitalWrite(PH0, direction);
      ledcWriteChannel(pwm_channels[0], pulse);
      break;
    case 1:
      digitalWrite(PH1, direction);
      ledcWriteChannel(pwm_channels[1], pulse);
      break;
    case 2:
      digitalWrite(PH2, direction);
      ledcWriteChannel(pwm_channels[2], pulse);
      break;
    case 3:
      digitalWrite(PH3, direction);
      ledcWriteChannel(pwm_channels[3], pulse);
      break;
  }
}

/**
 * @brief 指定したサーボの角度を設定
 * @param servo_index 0, 1, 2, 3 (SR0, SR1, SR2, SR3)
 * @param angle 0.0 〜 180.0
 */
void RobotHardware::setServoAngle(int servo_index, float angle) {
  // angleを 0.0 〜 180.0 の範囲に制限
  angle = constrain(angle, 0.0, 180.0);
  
  int pulse = servoAngle(angle, SERVO_PWM_RESOLUTION); // PWMパルス幅に変換

  switch (servo_index) {
    case 0:
      ledcWriteChannel(pwm_channels[4], pulse); // SR0
      break;
    case 1:
      ledcWriteChannel(pwm_channels[5], pulse); // SR1
      break;
    case 2:
      ledcWriteChannel(pwm_channels[6], pulse); // SR2
      break;
    case 3:
      ledcWriteChannel(pwm_channels[7], pulse); // SR3
      break;
  }
}

/**
 * @brief 指定したLEDを点灯/消灯
 * @param led_index 0, 1, 2
 * @param state true (ON) または false (OFF)
 */
void RobotHardware::setLED(int led_index, bool state) {
  int pin = -1;
  switch (led_index) {
    case 0:
      pin = LED0;
      break;
    case 1:
      pin = LED1;
      break;
    case 2:
      pin = LED2;
      break;
  }
  
  if (pin != -1) {
    digitalWrite(pin, state ? HIGH : LOW);
  }
}

float RobotHardware::getDuty(int motor_index){
  if (motor_index >= 0 && motor_index < 4) {
    return duty[motor_index];
  }
  return 0.0;
}


// --- プライベート ヘルパー関数 ---
// (Rikoten_mainboard_test.ino から移植)

/**
 * @brief デューティ比 (0.0~1.0) をPWMパルス幅 (0~max) に変換
 */
int RobotHardware::duty2Pulse(float duty, int resolution) {
  return (int)(duty * (pow(2, resolution) - 1));
}

/**
 * @brief 角度 (0~180) をサーボ用PWMパルス幅 (0~4095) に変換
 */
int RobotHardware::servoAngle(float angle, int resolution) {
  // 50Hz (20ms周期) の場合
  float min_duty = (float)500 / 20000;  // 0.5ms
  float max_duty = (float)2400 / 20000; // 2.4ms
  float duty = ((max_duty - min_duty) / 180.0) * angle + min_duty;
  return duty2Pulse(duty, resolution);
}