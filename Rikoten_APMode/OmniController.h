#ifndef OMNI_H
#define OMNI_H

#include <math.h>
#include "robot_params.h"

//================================================
// オムニホイール制御クラス
//================================================
template<int NUM_WHEELS> // ★テンプレート化してホイール数(NUM_WHEELS)を可変に
class OmniController {
public:
  // コンストラクタ
  OmniController(float robot_radius, float wheel_radius, const float wheel_angles_deg[NUM_WHEELS]) {
    m_robot_radius = robot_radius;
    // ★追加: 割り算を避けるため、半径の逆数を計算して保持
    if (wheel_radius != 0) {
      m_inv_wheel_radius = 1.0 / wheel_radius;
    }

    // 各ホイールの角度のsinとcosを一度だけ計算して保存
    for (int i = 0; i < NUM_WHEELS; i++) { // ★マジックナンバー'3'を排除
      float angle_rad = wheel_angles_deg[i] * M_PI / 180.0;
      m_sin_angles[i] = sin(angle_rad);
      m_cos_angles[i] = cos(angle_rad);
    }
  }

  // 目標速度を設定する
  void setTargetVelocity(float vx, float vy, float omega) {
    m_target_vx = vx;
    m_target_vy = vy;
    m_target_omega = omega;
  }

  // ホイールの目標角速度[rad/s]を計算する
  void calculateWheelSpeeds() {
    const float L = m_robot_radius;

    for (int i = 0; i < NUM_WHEELS; i++) { // ★マジックナンバー'3'を排除
      // まずホイールの接線速度[m/s]を計算
      float tangential_velocity = m_target_vy * m_cos_angles[i] - m_target_vx * m_sin_angles[i] + L * m_target_omega;
      
      // ★追加: 接線速度を角速度[rad/s]に変換 (ω = v / r)
      m_wheel_speeds_rad_s[i] = tangential_velocity * m_inv_wheel_radius;
    }
  }

  // 計算されたホイールの角速度[rad/s]を取得する
  float getWheelSpeed(int wheel_index) {
    if (wheel_index >= 0 && wheel_index < NUM_WHEELS) {
      return m_wheel_speeds_rad_s[wheel_index];
    }
    return 0.0;
  }

private:
  float m_robot_radius;
  float m_inv_wheel_radius; // ★追加
  float m_target_vx, m_target_vy, m_target_omega;
  
  // ★配列のサイズを可変に
  float m_wheel_speeds_rad_s[NUM_WHEELS];
  float m_sin_angles[NUM_WHEELS];
  float m_cos_angles[NUM_WHEELS];
};

#endif // OMNI_H