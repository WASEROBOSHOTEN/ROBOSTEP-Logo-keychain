# CH32V003使用 ROBOSTEP ロゴキーホルダー

## 概要

CH32V003J4M6を使用し、わずか5本のGPIOピンで20個のLEDを制御するプロジェクトです。

**チャーリープレクシング (Charlieplexing)** 回路構成と、タイマー割り込みによる**ダイナミック点灯**（残像効果）を組み合わせて、複雑なLEDアニメーションを実現しています。

## ハードウェア構成

* **MCU:** CH32V003J4M6
* **LED制御 (5ピン):**
    * `GPIOA, GPIO_Pin_1` (PA1)
    * `GPIOA, GPIO_Pin_2` (PA2)
    * `GPIOC, GPIO_Pin_1` (PC1)
    * `GPIOC, GPIO_Pin_2` (PC2)
    * `GPIOC, GPIO_Pin_4` (PC4)
* **LED数:** 20個
* **入力:** `GPIOD, GPIO_Pin_1` (PD1) に接続されたタクトスイッチ (モード切替用)

## 開発環境

* **IDE:** MounRiver Studio

## 使用方法

このリポジトリは `main.c` のみを提供しています。プロジェクトをビルドするには、以下の手順に従ってください。

1.  **MounRiver Studio** で CH32V003 用の新しいプロジェクトを作成します。
2.  プロジェクトが自動生成した `main.c` の中身を、このリポジトリの `main.c` の内容で置き換えてください。
3.  下記「注意事項」に従い、システムクロックの設定を変更してください。
4.  ビルドして書き込みます。

## 注意事項：システムクロックの設定

このコードは、`SysTick` と `TIM1` のタイマーが **48MHz** のシステムクロック（SYSCLK）で動作することを前提に設計されています。

MounRiver Studioが生成するデフォルトの `system_ch32v00x.c` ファイルを開き、システムクロックの定義を **48MHz (HSI)** に変更する必要があります。

**変更前:**
```c
//#define SYSCLK_FREQ_8MHz_HSI    8000000
//#define SYSCLK_FREQ_24MHZ_HSI   HSI_VALUE
#define SYSCLK_FREQ_48MHZ_HSI   48000000  //この行をコメントアウト
//#define SYSCLK_FREQ_8MHz_HSE    8000000
//#define SYSCLK_FREQ_24MHz_HSE   HSE_VALUE
// #define SYSCLK_FREQ_48MHz_HSE   48000000
