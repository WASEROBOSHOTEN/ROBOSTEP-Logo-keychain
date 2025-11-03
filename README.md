# ESP32 オムニホイール & サーボアーム Webコントローラ

スマートフォンからWi-Fi経由で操作できる、3輪オムニホイールロボットと2軸サーボアームの制御プロジェクトです。

ESP32がWi-Fiアクセスポイントとして動作し、Webブラウザ（iOS/Android対応）にコントローラのUIを提供します。  
通信はなWebSocketで行われ、リアルタイムな操作とテレメトリの受信が可能です。

---

## 🚀 使い方 (セットアップ)

### 1. Arduino IDE のセットアップ

1.  **ESP32ボード定義のインストール:**
    * Arduino IDEの「環境設定」>「追加のボードマネージャURL」に以下を追加します。
        `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
    * 「ボードマネージャ」で `esp32` を検索し、インストールします。

2.  **必要なライブラリのインストール:**
    * 「ライブラリを管理」から以下のライブラリをインストールします。
        * `ESPAsyncWebServer`
        * `AsyncTCP`
        * `ArduinoJson`

3.  **LittleFS Uploaderのインストール:**
    * [ESP32 LittleFS Uploader](https://github.com/earlephilhower/arduino-littlefs-upload) のリリースページから `ESP32FS-*.zip` をダウンロードします。
    * Arduinoの `tools` フォルダ（例: `Documents/Arduino/tools/`）に解凍して配置し、Arduino IDEを再起動します。  
    [こちらのサイトも参照してください。](https://qiita.com/kumakumao/items/be51f174bfeb0e4a6a06)  

### 2. UIファイルの書き込み

1.  このプロジェクトの `data` フォルダ内に `index.html` と `nipplejs.min.js` を配置します。
2.  Arduino IDEでCtrl+Shift+Pでコマンドパレットを開き、「Upload LittleFS to Pico/ESP8266/ESP32」を実行します。
3.  data配下のファイルが自動的にアップロードされ、CompletedUploadと表示されれば完了です。

### 3. スケッチの書き込み

1.  `Rikoten_APMode.ino`を開きます。
2.  **[ツール] > [ボード]** で `ESP32 Dev Module` などを選択します。
3.  「→」（マイコンに書き込む）ボタンを押します。
4.  ESP32基板の `BOOT` (GPIO 0) ボタンを押しながら `RESET` (EN) ボタンを押してdowndoal modeに入ります。
5.  書き込みの終了後，ESPをリセットすると動作が始まります。

### 4. 操作方法

1.  ESP32の電源を入れます。
2.  スマートフォンやPCのWi-Fi設定を開き、`ESP_Robot_WiFi` に接続します。（パスワード: `1234567890`）
3.  任意のWebブラウザを開き、アドレスバーに `192.168.4.1` と入力します。
4.  コントローラUIが表示されます。

---

## 🛠️ システム構成

1.  **スマートフォン (クライアント):**
    * `ESP_Robot_WiFi` に接続し、`192.168.4.1` にアクセス。
    * `index.html` のUIを操作し、`controlState` JSONなどをWebSocketで送信。
2.  **ESP32 (サーバー):**
    * `WebServerManager` がWebSocketでJSONを受信し、グローバル変数を更新。
    * `Rikoten_APMode.ino` の `Ticker` (`updateControl`) が10msごとにグローバル変数を読み取る。
    * `OmniController` が `vx, vy, omega` を3輪の `power` 値に変換。
    * `RobotHardware` が `power` 値をデッドゾーン補正 し、`ledcWrite` でモーターを駆動。
    * `RobotHardware` がサーボの角度を `setServoAngle` で制御。

---

## 📂 プロジェクトのファイル構成  

<pre>
Rikoten_APMode/
├── Rikoten_APMode.ino        // メインスケッチ、Ticker制御ループ
├── robot_params.h            // 全てのピン設定、物理パラメータ、チューニング値
├── OmniController.h          // オムニホイールの運動学クラス
├── RobotHardware.h           // ハードウェア（モーター、サーボ、LED）の抽象化クラス
├── RobotHardware.cpp         // 
├── WebServerManager.h        // WebサーバーとWebSocketの管理クラス
├── WebServerManager.cpp      //
└── data/                     // LittleFSに書き込むデータ
    ├── index.html            // コントローラUI本体
    └── nipplejs.min.js       // UI用JavaScriptライブラリ
</pre>
