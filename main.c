// system_ch32v00x.c は SYSCLK_FREQ_48MHZ_HSI 48000000 を使用想定

#include "debug.h"
#include "math.h" // calculate_duration_ease_out_sqrt で使用
#include "string.h"
#include "stdlib.h"

// ピンの状態を定義
typedef enum {
    PIN_STATE_HIGH, // 出力 HIGH
    PIN_STATE_LOW,  // 出力 LOW
    PIN_STATE_HIZ,  // ハイインピーダンス (入力)
    PIN_STATE_IPU
} PinState_t;

// --- グローバル変数 ---
volatile uint32_t g_systick_ms = 0; // 1msカウンタ (SysTickで更新)
volatile uint8_t mode = 0;          // 現在のモード

volatile uint8_t leds_to_display[5] = {0};

// TIM1割り込みハンドラ用カウンタ
volatile uint8_t dynamic_drive_counter = 0;

// TIM1ハンドラが参照する同時点灯数を制御する変数
volatile uint8_t LED_volume = 1; // 初期値1

// --- LED制御関数 ---
/**
 * @brief (高速版) 指定したピンを指定した状態に設定する
 * @note GPIOx->CFGLRレジスタ(モード設定)とGPIOx->BSHR(出力設定)を直接操作します。
 * GPIO_Init() を呼び出さないため、割り込みハンドラ内での使用に最適です。
 */
void set_pin_state(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, PinState_t state)
{
    // HIZ (Input Floating) = 0100 (0x4)
    // Output PP 50MHz     = 0011 (0x3)
    uint32_t cnf_mode_bits = (state == PIN_STATE_HIGH || state == PIN_STATE_LOW) ? 0x3 : 0x4;

    // --- どのピンか判別し、CFGLRレジスタ(モード設定)を操作 ---
    // (GPIOA->CFGLR & ~(マスク)) で該当ピンのビットをクリアし、
    // | (設定値) で新しいモードを設定します。
    if (GPIOx == GPIOA) {
        if (GPIO_Pin == GPIO_Pin_1) { // PA1 (CFGLR[7:4])
            GPIOA->CFGLR = (GPIOA->CFGLR & ~(0xF << 4)) | (cnf_mode_bits << 4);
        }
        else if (GPIO_Pin == GPIO_Pin_2) { // PA2 (CFGLR[11:8])
            GPIOA->CFGLR = (GPIOA->CFGLR & ~(0xF << 8)) | (cnf_mode_bits << 8);
        }
    }
    else if (GPIOx == GPIOC) {
        if (GPIO_Pin == GPIO_Pin_1) { // PC1 (CFGLR[7:4])
            GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xF << 4)) | (cnf_mode_bits << 4);
        }
        else if (GPIO_Pin == GPIO_Pin_2) { // PC2 (CFGLR[11:8])
            GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xF << 8)) | (cnf_mode_bits << 8);
        }
        else if (GPIO_Pin == GPIO_Pin_4) { // PC4 (CFGLR[19:16])
            GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xF << 16)) | (cnf_mode_bits << 16);
        }
    }

    // --- 出力状態をBSHRレジスタで高速に設定 ---
    // BSHRレジスタはアトミックな(読み書き競合しない)操作が可能です
    if (state == PIN_STATE_HIGH) {
        GPIOx->BSHR = GPIO_Pin; // Set
    } else if (state == PIN_STATE_LOW) {
        GPIOx->BSHR = (uint32_t)GPIO_Pin << 16; // Reset
    }
    // HIZ/IPUの場合はBSHRを操作しない (モード設定で出力が切断されるため)
}



/**
 * @brief 指定された番号のLEDを点灯させる (Charlieplexing)
 */
void setLED(int number){
    // 注意: この関数は set_pin_state を複数回呼び出すため、
    // TIM1割り込みハンドラ内で直接呼び出すと時間がかかる可能性があります。
    // ゴースト対策の全ピンHIZ設定はハンドラ側で行います。
    switch (number) {
        case 1:  set_pin_state(GPIOA, GPIO_Pin_1, PIN_STATE_HIGH); set_pin_state(GPIOA, GPIO_Pin_2, PIN_STATE_LOW); break;
        case 2:  set_pin_state(GPIOA, GPIO_Pin_1, PIN_STATE_LOW);  set_pin_state(GPIOA, GPIO_Pin_2, PIN_STATE_HIGH); break;
        case 3:  set_pin_state(GPIOA, GPIO_Pin_2, PIN_STATE_HIGH); set_pin_state(GPIOC, GPIO_Pin_1, PIN_STATE_LOW); break;
        case 4:  set_pin_state(GPIOA, GPIO_Pin_2, PIN_STATE_LOW);  set_pin_state(GPIOC, GPIO_Pin_1, PIN_STATE_HIGH); break;
        case 5:  set_pin_state(GPIOC, GPIO_Pin_1, PIN_STATE_HIGH); set_pin_state(GPIOC, GPIO_Pin_2, PIN_STATE_LOW); break;
        case 6:  set_pin_state(GPIOC, GPIO_Pin_1, PIN_STATE_LOW);  set_pin_state(GPIOC, GPIO_Pin_2, PIN_STATE_HIGH); break;
        case 7:  set_pin_state(GPIOC, GPIO_Pin_2, PIN_STATE_HIGH); set_pin_state(GPIOC, GPIO_Pin_4, PIN_STATE_LOW); break;
        case 8:  set_pin_state(GPIOC, GPIO_Pin_2, PIN_STATE_LOW);  set_pin_state(GPIOC, GPIO_Pin_4, PIN_STATE_HIGH); break;
        case 9:  set_pin_state(GPIOA, GPIO_Pin_1, PIN_STATE_HIGH); set_pin_state(GPIOC, GPIO_Pin_1, PIN_STATE_LOW); break;
        case 10: set_pin_state(GPIOA, GPIO_Pin_1, PIN_STATE_LOW);  set_pin_state(GPIOC, GPIO_Pin_1, PIN_STATE_HIGH); break;
        case 11: set_pin_state(GPIOA, GPIO_Pin_1, PIN_STATE_LOW);  set_pin_state(GPIOC, GPIO_Pin_4, PIN_STATE_HIGH); break;
        case 12: set_pin_state(GPIOA, GPIO_Pin_1, PIN_STATE_HIGH); set_pin_state(GPIOC, GPIO_Pin_4, PIN_STATE_LOW); break;
        case 13: set_pin_state(GPIOA, GPIO_Pin_2, PIN_STATE_LOW);  set_pin_state(GPIOC, GPIO_Pin_4, PIN_STATE_HIGH); break;
        case 14: set_pin_state(GPIOA, GPIO_Pin_2, PIN_STATE_HIGH); set_pin_state(GPIOC, GPIO_Pin_4, PIN_STATE_LOW); break;
        case 15: set_pin_state(GPIOA, GPIO_Pin_2, PIN_STATE_LOW);  set_pin_state(GPIOC, GPIO_Pin_2, PIN_STATE_HIGH); break;
        case 16: set_pin_state(GPIOA, GPIO_Pin_2, PIN_STATE_HIGH); set_pin_state(GPIOC, GPIO_Pin_2, PIN_STATE_LOW); break;
        case 17: set_pin_state(GPIOA, GPIO_Pin_1, PIN_STATE_LOW);  set_pin_state(GPIOC, GPIO_Pin_2, PIN_STATE_HIGH); break;
        case 18: set_pin_state(GPIOA, GPIO_Pin_1, PIN_STATE_HIGH); set_pin_state(GPIOC, GPIO_Pin_2, PIN_STATE_LOW); break;
        case 19: set_pin_state(GPIOC, GPIO_Pin_1, PIN_STATE_LOW);  set_pin_state(GPIOC, GPIO_Pin_4, PIN_STATE_HIGH); break;
        case 20: set_pin_state(GPIOC, GPIO_Pin_1, PIN_STATE_HIGH); set_pin_state(GPIOC, GPIO_Pin_4, PIN_STATE_LOW); break;
        // default は何もしない (全ピンHIZ想定)
    }
}

/*********************************************************************
 * @fn      calculate_duration
 * @brief   線形加速/減速する際の特定のステップにおける持続時間を計算します。
 * @param   start_ms - 最初のステップ (インデックス 0相当) の持続時間 (ミリ秒)。
 * @param   end_ms - 最後のステップの持続時間 (ミリ秒)。
 * @param   total_steps - シーケンスの総ステップ数 (例: 10ステップなら10)。
 * @param   current_step_in_segment - セグメント内の現在のステップ (0 から total_steps - 1)。
 * @return  計算された現在のステップの持続時間 (ミリ秒)。
 */
uint32_t calculate_duration(uint16_t start_ms, uint16_t end_ms, uint8_t total_steps, uint8_t current_step_in_segment)
{
    // ステップが1つしかない場合は割り算を避ける
    if (total_steps <= 1) {
        return start_ms;
    }
    // current_step_in_segment が範囲外の場合のエラー処理 (念のため)
    if (current_step_in_segment >= total_steps) {
        current_step_in_segment = total_steps - 1;
    }

    // 線形補間を使用して持続時間を計算
    int32_t duration = (int32_t)start_ms +
                       ((int32_t)end_ms - (int32_t)start_ms) * (int32_t)current_step_in_segment / ((int32_t)total_steps - 1);

    // 範囲チェックと最低持続時間の保証
    if (start_ms >= end_ms) { // 加速
        if (duration < end_ms) duration = end_ms;
        if (duration > start_ms) duration = start_ms;
    } else { // 減速
         if (duration > end_ms) duration = end_ms;
         if (duration < start_ms) duration = start_ms;
    }
    if (duration < 1) duration = 1;

    return (uint32_t)duration;
}

// --- タイマー割り込みハンドラ ---

/**
 * @brief SysTick割り込みハンドラ (1msごとに呼ばれる)
 */
void SysTick_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void SysTick_Handler(void)
{
    g_systick_ms++;
    SysTick->SR = 0;
}

/**
 * @brief TIM1割り込みハンドラ (ダイナミック点灯用)
 */
void TIM1_UP_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM1_UP_IRQHandler(void)
{
    if(TIM_GetITStatus(TIM1, TIM_IT_Update) == SET)
    {
        uint8_t led_num = 0;
        // LED_volume は 1 から 5 の範囲と想定
        if (LED_volume > 0 && dynamic_drive_counter < LED_volume && dynamic_drive_counter < 5) {
             led_num = leds_to_display[dynamic_drive_counter];
        }

        // --- (高速化) ゴースト対策: 先に全ピンをHIZにする ---
        // GPIO_Init()の代わりにCFGLRレジスタを直接操作
        // HIZ (Input Floating) = 0100 (0x4)
        
        // PA1(Bit[7:4]), PA2(Bit[11:8]) を HIZ (0x4) に設定
        GPIOA->CFGLR = (GPIOA->CFGLR & ~((0xF << 4) | (0xF << 8)))
                                   | ((0x4 << 4) | (0x4 << 8));
        
        // PC1(Bit[7:4]), PC2(Bit[11:8]), PC4(Bit[19:16]) を HIZ (0x4) に設定
        GPIOC->CFGLR = (GPIOC->CFGLR & ~((0xF << 4) | (0xF << 8) | (0xF << 16)))
                                   | ((0x4 << 4) | (0x4 << 8) | (0x4 << 16));
        // --- ここまで高速化 ---

        if (led_num > 0 && led_num <= 20) {
            setLED(led_num); // 指定されたLEDを点灯 (内部で高速化された set_pin_state が呼ばれる)
        }

        // 次に光らせるLEDのインデックスに更新 (0 から LED_volume-1 をループ)
        dynamic_drive_counter++;
        if (dynamic_drive_counter >= LED_volume || LED_volume == 0) {
            dynamic_drive_counter = 0;
        }
    }
    TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
}


// --- 初期化関数 ---

/**
 * @brief TIM1初期化
 */
void TIM1_INT_Init( u16 arr, u16 psc)
{
    NVIC_InitTypeDef NVIC_InitStructure={0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure={0};
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE );
    TIM_TimeBaseInitStructure.TIM_Period = arr;
    TIM_TimeBaseInitStructure.TIM_Prescaler = psc;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit( TIM1, &TIM_TimeBaseInitStructure);
    NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1; // SysTickより優先度を低くする
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);
}

/**
 * @brief ボード全体の初期化
 */
void BoardInit(void){
    // 基本的な初期化
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init(); // set_pin_state内でGPIO_Initを使う場合に必要

    // クロック供給
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC, ENABLE);

    // PD1 (スイッチ入力) の初期化
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // SysTickタイマーを1ms間隔で設定
    NVIC_EnableIRQ(SysTick_IRQn);
    SysTick->SR &= ~(1 << 0);
    SysTick->CMP = SystemCoreClock / 1000 - 1; // 48MHz / 1000 = 48000
    SysTick->CNT = 0;
    SysTick->CTLR = 0xF;

    // TIM1をダイナミック点灯用に設定 (例: 約1ms周期 = 1kHz)
    // 24MHz / (24 * 1000) = 1kHz
    TIM1_INT_Init(500 - 1, 24 - 1);    //500
    TIM_Cmd( TIM1, ENABLE );
}

// --- メイン関数 ---
int main(void)
{
    BoardInit();    // 各種初期化 (SysTick, TIM1含む)
    srand(g_systick_ms); // ★ 乱数シードを初期化 (起動時のsystickで初期化)

    // --- チャタリング対策用の変数を定義 ---
    uint8_t switch_on_counter = 0;
    uint8_t switch_state = 1;
    const uint8_t SWITCH_THRESHOLD = 50; // ms
    const uint8_t SWITCH_SAMPLING_MS = 5; // ms

    // --- 時間管理とアニメーション状態の変数 ---
    const uint8_t MODE_VARS = 10; // ★ モード数 0-5 (計6種類) ★
    uint32_t last_anim_time = 0; // アニメーションステップ更新用
    uint32_t last_sample_time = 0; // スイッチサPLING用

    // 各アニメーションで使用する状態変数 (モード切替時にリセット)
    uint8_t anim_index = 0; // path配列のインデックス (0-19) や他の用途
    uint8_t anim_index_top = 10; // case 6 用
    uint8_t anim_index_bottom = 0; // case 6 用
    uint8_t anim_step = 1;  // case 0, 2 で使用
    uint8_t anim_dir = 0;   // アニメーションの方向/状態管理用

    uint8_t sparkle_leds[3] = {0}; // ★ case 7 (スパークル) 用のLED配列 ★

    // 物理的な反時計回りのLED番号の並び順
    const uint8_t path[20] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,  // index 0-9
                              20,19,18,17,16,15,14,13,12, 11}; // index 10-19

    // mode = 6; // デバッグ用

    while (1)
    {
        uint32_t current_time = g_systick_ms; // 現在時刻を一度だけ取得

        // --- スイッチ処理 ---
        if((current_time - last_sample_time) >= SWITCH_SAMPLING_MS){
            if (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_1) == Bit_RESET) {
                if(switch_on_counter < 255) switch_on_counter++;
            } else {
                switch_on_counter = 0;
            }
            last_sample_time = current_time;
        }

        if (switch_on_counter >= SWITCH_THRESHOLD/SWITCH_SAMPLING_MS) {
            if (switch_state == 1) { // 押された瞬間のみ
                mode++;
                if (mode >= MODE_VARS) mode = 0;

                // --- ★★★ モード切り替え時の状態リセット (単純化) ★★★ ---
                anim_index = 0;        // デフォルト開始インデックス
                anim_step = 1;         // デフォルト開始ステップ
                anim_dir = 0;          // デフォルト方向/状態
                anim_index_top = 19;   // case 6 用デフォルト
                anim_index_bottom = 0; // case 6 用デフォルト

                if (mode == 5) { // case 5 (同時落下バウンド 19->10, 0->9) - case番号注意
                    anim_index_top = 19;
                    anim_index_bottom = 0;
                    anim_dir = 0;
                }
                else if (mode == 6) { // ★ case 6 (振り子) 用の初期値を追加 ★
                    // anim_index_top = 19;    // 上半分の開始位置 (path[19] = LED 11)
                    anim_index_top = 10;
                    anim_index_bottom = 0;  // 下半分の開始位置 (path[0] = LED 1)
                    anim_dir = 0;           // 0 = 増加方向 (0->9)
                }
                else if (mode == 7) { // ★ case 7 (スパークル) 用の初期化 ★
                    // 最初のフレームを生成しておく
                    sparkle_leds[0] = path[rand() % 20];
                    sparkle_leds[1] = path[rand() % 20];
                    sparkle_leds[2] = path[rand() % 20];
                }
                else if (mode == 8) { // ★ case 8 (コメット) 用 ★
                    anim_index = 0; // デフォルトだが明示
                }
                // --- ★★★ ここまでリセット処理 ★★★ ---

                switch_state = 0; // 押されている状態に
                last_anim_time = current_time; // アニメーション時間もリセット
            }
        } else {
            switch_state = 1;
        }
        // --- ここまでスイッチ処理 ---

        uint8_t current_led_count = 0; // このループで TIM ハンドラに渡すLED数

        memset((void*)leds_to_display, 0, sizeof(leds_to_display));

        switch(mode)
        {
            case 0: // 向かい合わせ
            {
                uint32_t required_interval = 50;
                // 1. 表示設定 (毎回実行)
                current_led_count = 2;
                uint8_t current_led_num = (anim_step >= 1 && anim_step <= 10) ? anim_step : 1;
                leds_to_display[0] = current_led_num;
                leds_to_display[1] = current_led_num + 10;

                // 2. 時間経過チェックと状態更新
                if((current_time - last_anim_time) >= required_interval) {
                    anim_step++;
                    if(anim_step > 10) anim_step = 1;
                    last_anim_time = current_time;
                }
            }
                break;

            case 1: // path配列を1周 (単一LED)
            {
                uint32_t required_interval = 20;
                // 1. 表示設定 (毎回実行)
                current_led_count = 1;
                if (anim_index < 20) { leds_to_display[0] = path[anim_index]; }

                // 2. 時間経過チェックと状態更新
                if((current_time - last_anim_time) >= required_interval) {
                    anim_index++;
                    if(anim_index >= 20) anim_index = 0;
                    last_anim_time = current_time;
                }
            }
                break;

            case 2: // バウンス (1-5往復、4LED)
            {
                uint32_t required_interval_c2;
                if (anim_dir == 2) { required_interval_c2 = 1000; } else { required_interval_c2 = 75; }

                // 1. 表示設定 (毎回実行)
                if (anim_step != 0 && anim_dir != 2) { // Pause中でなければ
                    leds_to_display[0] = anim_step; // anim_step は 1-5
                    leds_to_display[1] = 11 - anim_step;
                    leds_to_display[2] = 10 + anim_step;
                    leds_to_display[3] = 21 - anim_step;
                    current_led_count = 4;
                } else {
                    current_led_count = 0;
                }

                // 2. 時間経過チェックと状態更新
                if((current_time - last_anim_time) >= required_interval_c2) {
                    if (anim_dir == 0) { anim_step++; if (anim_step > 5) { anim_step = 4; anim_dir = 1; } }
                    else if (anim_dir == 1) { anim_step--; if (anim_step < 1) { anim_step = 0; anim_dir = 2; } }
                    else { anim_step = 1; anim_dir = 0; }
                    last_anim_time = current_time;
                }
            }
                break;

            case 3: // path配列を1周 (3LED等間隔)
            {
                uint32_t required_interval = 50;
                // 1. 表示設定 (毎回実行)
                current_led_count = 3;
                if (anim_index < 20) {
                    uint8_t index1 = anim_index;
                    uint8_t index2 = (anim_index + 7) % 20;
                    uint8_t index3 = (anim_index + 14) % 20;
                    leds_to_display[0] = path[index1];
                    leds_to_display[1] = path[index2];
                    leds_to_display[2] = path[index3];
                    leds_to_display[3] = 0; //明示しないとゴーストが出る
                }

                // 2. 時間経過チェックと状態更新
                if((current_time - last_anim_time) >= required_interval) {
                    anim_index++;
                    if (anim_index >= 20) anim_index = 0;
                    last_anim_time = current_time;
                }
            }
                break;

            
            case 4: // 2段階加速アニメーション
            {
                uint32_t required_interval_c4;
                const uint8_t segment_steps = 10;
                const uint16_t start_duration_c4 = 100;
                const uint16_t end_duration_c4 = 5;

                // 1. 表示設定 (毎回実行)
                if ((anim_dir == 0 || anim_dir == 2) && anim_index < 20) { // Moving
                    leds_to_display[0] = path[anim_index];
                    current_led_count = 1;
                } else { // Paused
                    current_led_count = 0;
                }

                // 2. 時間経過チェックと状態更新
                switch(anim_dir) {
                    case 0: required_interval_c4 = calculate_duration(start_duration_c4, end_duration_c4, segment_steps, anim_index); break;
                    case 2: required_interval_c4 = calculate_duration(start_duration_c4, end_duration_c4, segment_steps, anim_index - 10); break;
                    case 1: case 3: default: required_interval_c4 = 1000; break;
                }

                if((current_time - last_anim_time) >= required_interval_c4) {
                    switch(anim_dir) {
                        case 0: if (anim_index == (segment_steps - 1)) { anim_dir = 1; } else { anim_index++; } break;
                        case 1: anim_index = 10; anim_dir = 2; break;
                        case 2: if (anim_index == (segment_steps * 2 - 1)) { anim_dir = 3; } else { anim_index++; } break;
                        case 3: default: anim_index = 0; anim_dir = 0; break;
                    }
                    last_anim_time = current_time;
                }
            }
                break; // case 4 の終了

            case 5: // ★ 上下同時落下?バウンド ★
            {
                uint32_t required_interval_c5 = 100; // 変数名を _c5 に統一
                uint8_t current_segment_step = 0;
                uint8_t total_segment_steps = 1;

                // アニメーションパラメータ
                const uint16_t fall_start_ms = 100; // 落下速度 (必要に応じて調整)
                const uint16_t fall_end_ms   = 20;
                const uint16_t blink_ms      = 50;
                const uint16_t pause_ms      = 1000;

                // バウンドの頂点?底インデックス
                const uint8_t top_bottom_index = 10; // 上段の底 (path[10]=LED 20)
                const uint8_t bottom_bottom_index = 9; // 下段の底 (path[9]=LED 10)
                // バウンドの高さ (底からのオフセット)
                const uint8_t bounce1_offset = 4;
                const uint8_t bounce2_offset = 2;
                const uint8_t bounce3_offset = 1;

                // 1. 表示設定 (毎回実行)
                current_led_count = 0;
                // anim_dir: 0=落下, 1=B1↑, 2=B1↓, ..., 7=Blink1ON, 8=Blink1OFF, 9=Blink2ON, 10=Blink2OFF, 11=Pause
                if (anim_dir <= 6) { // 移動中
                    if (anim_index_top >= top_bottom_index && anim_index_top < 20) { leds_to_display[current_led_count++] = path[anim_index_top]; }
                    if (anim_index_bottom <= bottom_bottom_index && anim_index_bottom < 10) { leds_to_display[current_led_count++] = path[anim_index_bottom]; }
                } else if (anim_dir == 7 || anim_dir == 9) { // Blink ON
                    leds_to_display[0] = path[top_bottom_index];
                    leds_to_display[1] = path[bottom_bottom_index];
                    current_led_count = 2;
                } // Blink OFF / Pause は current_led_count = 0 のまま

                // 2. 時間経過チェックと状態更新
                // 現在の状態に応じた時間間隔を計算 (下段の動きで代表させる)
                switch(anim_dir) {
                    case 0: total_segment_steps = 10; current_segment_step = anim_index_bottom; required_interval_c5 = calculate_duration(fall_start_ms, fall_end_ms, total_segment_steps, current_segment_step); break;
                    case 1: total_segment_steps = bounce1_offset + 1; current_segment_step = bottom_bottom_index - anim_index_bottom; required_interval_c5 = calculate_duration(fall_end_ms, fall_start_ms, total_segment_steps, current_segment_step); break;
                    case 2: total_segment_steps = bounce1_offset + 1; current_segment_step = anim_index_bottom - (bottom_bottom_index - bounce1_offset); required_interval_c5 = calculate_duration(fall_start_ms, fall_end_ms, total_segment_steps, current_segment_step); break;
                    case 3: total_segment_steps = bounce2_offset + 1; current_segment_step = bottom_bottom_index - anim_index_bottom; required_interval_c5 = calculate_duration(fall_end_ms, fall_start_ms, total_segment_steps, current_segment_step); break;
                    case 4: total_segment_steps = bounce2_offset + 1; current_segment_step = anim_index_bottom - (bottom_bottom_index - bounce2_offset); required_interval_c5 = calculate_duration(fall_start_ms, fall_end_ms, total_segment_steps, current_segment_step); break;
                    case 5: total_segment_steps = bounce3_offset + 1; current_segment_step = bottom_bottom_index - anim_index_bottom; required_interval_c5 = calculate_duration(fall_end_ms, fall_start_ms, total_segment_steps, current_segment_step); break;
                    case 6: total_segment_steps = bounce3_offset + 1; current_segment_step = anim_index_bottom - (bottom_bottom_index - bounce3_offset); required_interval_c5 = calculate_duration(fall_start_ms, fall_end_ms, total_segment_steps, current_segment_step); break;
                    case 7: case 8: case 9: case 10: required_interval_c5 = blink_ms; break;
                    case 11: default: required_interval_c5 = pause_ms; break;
                }

                if((current_time - last_anim_time) >= required_interval_c5) {
                    // 次の状態へ遷移 (上下同時に更新)
                    switch(anim_dir) {
                        case 0: if (anim_index_bottom == bottom_bottom_index) anim_dir = 1; else { anim_index_top--; anim_index_bottom++; } break;
                        case 1: if (anim_index_bottom == (bottom_bottom_index - bounce1_offset)) anim_dir = 2; else { anim_index_top++; anim_index_bottom--; } break;
                        case 2: if (anim_index_bottom == bottom_bottom_index) anim_dir = 3; else { anim_index_top--; anim_index_bottom++; } break;
                        case 3: if (anim_index_bottom == (bottom_bottom_index - bounce2_offset)) anim_dir = 4; else { anim_index_top++; anim_index_bottom--; } break;
                        case 4: if (anim_index_bottom == bottom_bottom_index) anim_dir = 5; else { anim_index_top--; anim_index_bottom++; } break;
                        case 5: if (anim_index_bottom == (bottom_bottom_index - bounce3_offset)) anim_dir = 6; else { anim_index_top++; anim_index_bottom--; } break;
                        case 6: if (anim_index_bottom == bottom_bottom_index) anim_dir = 7; else { anim_index_top--; anim_index_bottom++; } break;
                        case 7: anim_dir = 8; break;
                        case 8: anim_dir = 9; break;
                        case 9: anim_dir = 10; break;
                        case 10: anim_dir = 11; break;
                        case 11: default: anim_index_top = 19; anim_index_bottom = 0; anim_dir = 0; break;
                    }
                    last_anim_time = current_time;
                }
            }
                break; // case 5 の終了
            
            case 6: // 振り子 (イージング修正版)
            {
                uint32_t required_interval_c6;
                uint8_t current_segment_step = 0;

                // 振り子の端（最も遅い）と中央（最も速い）の持続時間
                const uint16_t swing_slow_ms = 200;
                const uint16_t swing_fast_ms = 10;

                // 振り子の片道は10ステップ (0..9)
                // その半分のステップ数 (0..4 の 5ステップ) で加速/減速が完了する
                const uint8_t total_segment_steps = 5; // 0, 1, 2, 3, 4 の5段階

                // 1. 表示設定 (毎回実行)
                current_led_count = 2;
                if (anim_index_bottom < 10) { // 下半分 (0-9)
                    leds_to_display[0] = path[anim_index_bottom];
                }
                if (anim_index_top >= 10 && anim_index_top < 20) { // 上半分 (10-19)
                    leds_to_display[1] = path[anim_index_top];
                }

                // 2. 時間経過チェックと状態更新
                // anim_index_bottom (0..9) の位置に基づいて、
                // 端(0)からの経過ステップ数 (0..4) を計算する
                if (anim_index_bottom < 5) {
                    // 加速フェーズ (0 -> 4)
                    current_segment_step = anim_index_bottom; // 0, 1, 2, 3, 4
                } else {
                    // 減速フェーズ (5 -> 9)
                    current_segment_step = 9 - anim_index_bottom; // 4, 3, 2, 1, 0
                }
                
                // (端:slow, 中央:fast, ステップ数, 現在ステップ)
                // これにより、持続時間は (100, 82, 65, 47, 30, 30, 47, 65, 82, 100) のように変化する
                required_interval_c6 = calculate_duration(swing_slow_ms, swing_fast_ms, total_segment_steps, current_segment_step);


                if((current_time - last_anim_time) >= required_interval_c6) {
                    last_anim_time = current_time; // 時間を更新
                    if (anim_dir == 0) { // 増加方向 (下: 0->9, 上: 10->19)
                        if (anim_index_bottom >= 9) {
                            anim_dir = 1; // 折り返し (次から減少)
                            // 実際には9->8の更新は次のループ
                            anim_index_bottom--;
                            anim_index_top--; // ★ 変更 (++) -> (--)
                        } else {
                            anim_index_bottom++;
                            anim_index_top++; // ★ 変更 (--) -> (++)
                        }
                    } else { // 減少方向 (下: 9->0, 上: 19->10)
                        if (anim_index_bottom == 0) { // 0になったら
                            anim_dir = 0; // 折り返し (次から増加)
                            // 実際には0->1の更新は次のループ
                            anim_index_bottom++;
                            anim_index_top++; // ★ 変更 (--) -> (++)
                        } else {
                            anim_index_bottom--;
                            anim_index_top--; // ★ 変更 (++) -> (--)
                        }
                    }
                }
            }
                break; // case 6 の終了

            case 7: // スパークル (点滅版)
            {
                // --- アニメーション設定 ---
                const uint32_t SPARKLE_ON_MS = 50;  // 点灯している時間
                const uint32_t SPARKLE_OFF_MS = 80; // 消灯している時間
                uint32_t required_interval_c7;
                
                // 1. 状態に応じたインターバルとLED表示設定
                if (anim_dir == 0) {
                    // --- 点灯中の処理 ---
                    required_interval_c7 = SPARKLE_ON_MS;
                    
                    // 表示設定 (モード切替時 or 消灯->点灯時に生成されたLEDを表示)
                    current_led_count = 3;
                    leds_to_display[0] = sparkle_leds[0];
                    leds_to_display[1] = sparkle_leds[1];
                    leds_to_display[2] = sparkle_leds[2];

                } else {
                    // --- 消灯中の処理 ---
                    required_interval_c7 = SPARKLE_OFF_MS;
                    
                    // 表示設定 (消灯)
                    current_led_count = 0;
                }

                // 2. 時間経過チェックと状態更新
                if((current_time - last_anim_time) >= required_interval_c7) {
                    last_anim_time = current_time;
                    
                    if (anim_dir == 0) {
                        // 点灯 -> 消灯 へ
                        anim_dir = 1;
                    } else {
                        // 消灯 -> 点灯 へ
                        anim_dir = 0;
                        
                        // ★ 次に点灯するLEDをここでランダムに生成 ★
                        sparkle_leds[0] = path[rand() % 20];
                        sparkle_leds[1] = path[rand() % 20];
                        sparkle_leds[2] = path[rand() % 20];
                    }
                }
            }
                break; // case 7 の終了

            case 8: // コメット (3 LED)
            {
                // 彗星の速度 (ms)
                uint32_t required_interval_c8 = 50;
                
                // 1. 表示設定 (毎回実行)
                current_led_count = 3;
                if (anim_index < 20) { // 範囲チェック (念のため)
                    // (anim_index - 1 + 20) % 20 は、アンダーフローを防ぐための計算
                    uint8_t index_head = anim_index;
                    uint8_t index_tail1 = (anim_index - 1 + 20) % 20;
                    uint8_t index_tail2 = (anim_index - 2 + 20) % 20;
                    
                    leds_to_display[0] = path[index_head];
                    leds_to_display[1] = path[index_tail1];
                    leds_to_display[2] = path[index_tail2];
                }

                // 2. 時間経過チェックと状態更新
                if((current_time - last_anim_time) >= required_interval_c8) {
                    last_anim_time = current_time;
                    
                    anim_index++;
                    if (anim_index >= 20) anim_index = 0;
                }
            }
                break; // case 8 の終了

            case 9: // 5個ずつのグループ点灯
            {
                // 1000msごとに、点灯するグループを変える
                uint32_t required_interval_c9 = 500;
                
                // anim_index を 0, 1, 2, 3 のグループ番号として使用

                // 1. 表示設定 (毎回実行)
                current_led_count = 5;
                
                // anim_index (0-3) に応じて、path配列の開始オフセットを計算 (0, 5, 10, 15)
                uint8_t start_index = anim_index * 5;
                
                // leds_to_display 配列を埋める (5個)
                for (uint8_t i = 0; i < 5; i++) {
                    leds_to_display[i] = path[start_index + i];
                }

                // 2. 時間経過チェックと状態更新
                if((current_time - last_anim_time) >= required_interval_c9) {
                    last_anim_time = current_time;
                    
                    anim_index++;
                    if (anim_index >= 4) { // グループは 0, 1, 2, 3 の 4つ
                        anim_index = 0; // 4番目のグループが点灯したらリセット
                    }
                }
            }
                break; // case 9 の終了


            default: // 想定外のモードは消灯
                current_led_count = 0;
                break;
        } // switch(mode) の終了

        // TIMハンドラが参照するLED数を設定 (0の場合は最低1にする)
        // かつ、配列サイズ4を超えないように制限
        LED_volume = (current_led_count == 0) ? 1 : current_led_count;
        // if (LED_volume > 4) LED_volume = 4; // 最大4個に制限
        if (LED_volume > 5) LED_volume = 5; // 最大4個に制限

    } // while(1) の終了
} // main の終了
