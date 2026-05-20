# XIAO ESP32S3 Sense GNSS BNO08X Web Monitor

Seeed Studio XIAO ESP32S3 Senseを、GNSS取得、BNO08X姿勢取得、Webモニタリング、将来のSDログ/UART送信を担当するSense側マイコンとして使うためのPlatformIOプロジェクトです。

## 現在の機能

- GNSS NMEA受信
- GGA/RMC/GSA/GSV解析
- Fix状態、衛星数、緯度、経度、速度、進行方向、HDOP表示
- BNO08XのRoll/Pitch/Yaw取得
- GNSSとBNO08Xの同時動作
- `millis()`ベースの周期管理
- WiFiアクセスポイント作成
- Web UIによる状態監視
- `/api/status` JSON API
- microSD CSVログ
- GNSS未受信、IMU未接続でもWeb UIを継続

## 使用ボード

- Seeed Studio XIAO ESP32S3 Sense

PlatformIO environment:

```ini
[env:seeed_xiao_esp32s3]
board = seeed_xiao_esp32s3
framework = arduino
```

## 使用ライブラリ

- `adafruit/Adafruit BNO08x`
- `adafruit/Adafruit BusIO`
- `ESP32Async/AsyncTCP`
- `ESP32Async/ESPAsyncWebServer`

## 使用ピン

| 機能 | XIAO ESP32S3 GPIO |
| --- | --- |
| BNO08X SDA | D4 / GPIO5 |
| BNO08X SCL | D5 / GPIO6 |
| BNO08X INT | D3 / GPIO4 |
| BNO08X RST | D2 / GPIO3 |
| BNO08X I2C address | 0x4A fixed |
| GNSS MCU RX | GPIO1 |
| GNSS MCU TX | GPIO0 |
| SD SCK | GPIO7 |
| SD MISO | GPIO8 |
| SD MOSI | GPIO9 |
| SD CS | GPIO21 |

## 配線図

```text
XIAO ESP32S3 Sense        BNO08X
3V3 -------------------- VCC
GND -------------------- GND
D4 / GPIO5 ------------- SDA
D5 / GPIO6 ------------- SCL
D3 / GPIO4 ------------- INT
D2 / GPIO3 ------------- RST

XIAO ESP32S3 Sense        GNSS module
3V3 or 5V -------------- VCC
GND -------------------- GND
GPIO1 MCU RX ----------- GNSS TX
GPIO0 MCU TX ----------- GNSS RX

XIAO ESP32S3 Sense        microSD
GPIO7 ------------------- SCK
GPIO8 ------------------- MISO
GPIO9 ------------------- MOSI
GPIO21 ------------------ CS
```

GNSSモジュールの電源電圧は使用モジュールの仕様に合わせてください。

## BNO08X設定メモ

- I2Cアドレスは `0x4A` 固定です。
- 姿勢レポートの初期設定は `SH2_GAME_ROTATION_VECTOR` です。
- `AppConfig.h` の `kBnoUseArvrStabilizedReport` を `true` にすると `SH2_ARVR_STABILIZED_RV` に切り替えられます。
- BNO08X未応答時は段階的バックオフで再初期化します。

```text
1-2回目: 2秒
3-5回目: 5秒
6-10回目: 10秒
11回目以降: 30秒
```

## WiFi接続方法

XIAO ESP32S3 Senseは起動後、以下のアクセスポイントを作成します。

```text
SSID:     XIAO_BOAT_MONITOR
PASSWORD: 12345678
```

スマホまたはPCからこのWiFiへ接続してください。

## Web UIアクセス方法

ブラウザで以下にアクセスします。

```text
http://192.168.4.1/
```

表示項目:

- GNSS: Fix、衛星数、緯度、経度、速度、進行方向、HDOP
- BNO08X: Roll、Pitch、Yaw、UpdateHz
- BNO08X diagnostics: report type、accuracy、reset/retry/recovery counts
- SYSTEM: millis、free heap、WiFi接続数、loop周期
- SD Log: mount状態、ファイル名、書き込み行数、エラー数
- Satellites: 衛星システム、ID、使用中、CNO、仰角、方位角

Webページは1秒周期で自動更新します。

## JSON API仕様

Endpoint:

```text
GET /api/status
```

レスポンス例:

```json
{
  "gnss": {
    "fix": "3D",
    "nmeaReceived": true,
    "satellites": 12,
    "usedSatellites": 8,
    "hasLocation": true,
    "latitude": 35.0000000,
    "longitude": 139.0000000,
    "hasSpeed": true,
    "speedKmh": 1.23,
    "hasCourse": true,
    "courseDeg": 180.0,
    "hasHdop": true,
    "hdop": 0.90
  },
  "bno08x": {
    "initialized": true,
    "hasOrientation": true,
    "address": 74,
    "roll": 0.00,
    "pitch": 0.00,
    "yaw": 0.00,
    "updateHz": 10.0,
    "accuracy": 3,
    "reportType": "GAME_ROTATION_VECTOR",
    "resetCount": 0,
    "timeoutCount": 0,
    "initFailCount": 0,
    "reportFailCount": 0,
    "consecutiveFailCount": 0,
    "nextRetryDelayMs": 2000,
    "recoveryCount": 0,
    "lastResetAgeMs": 0,
    "lastTimeoutAgeMs": 0,
    "lastRecoveryAgeMs": 0
  },
  "system": {
    "millis": 123456,
    "freeHeap": 200000,
    "wifiClients": 1,
    "loopHz": 1000.0
  },
  "sd": {
    "mounted": true,
    "fileReady": true,
    "csPin": 21,
    "filePath": "/LOG000.CSV",
    "lastStatus": "logged",
    "writeCount": 123,
    "writeErrorCount": 0,
    "lastWriteAgeMs": 10,
    "lastErrorAgeMs": 0
  },
  "satellites": []
}
```

## SDカードCSVログ

SDカードがマウントできた場合、起動ごとに未使用のファイル名を自動採番します。

```text
/LOG000.CSV
/LOG001.CSV
...
```

既存ログは上書きしません。ログ周期は初期値1Hzです。

CSVの先頭にはフォーマットバージョンを記録します。

```text
# log_format_version,2
# project,XIAO ESP32S3 Sense GNSS BNO08X Web Monitor
# time/system,gnss,bno08x,imu_diag,future_control
```

主な列:

```text
millis,log_seq,
gnss_fix,gnss_sats,gnss_used_sats,gnss_has_location,
gnss_lat,gnss_lon,gnss_has_speed,gnss_speed_kmh,
gnss_has_course,gnss_course_deg,gnss_has_hdop,gnss_hdop,
bno_initialized,bno_has_orientation,bno_roll_deg,bno_pitch_deg,
bno_yaw_deg,bno_update_hz,
bno_reset_count,bno_timeout_count,bno_init_fail_count,bno_report_fail_count,
bno_accuracy_status,bno_report_type,bno_consecutive_fail,
bno_next_retry_delay_ms,bno_recovery_count,
system_heap,system_loop_hz,system_wifi_clients,
control_state,control_target_lat,control_target_lon,control_error
```

`control_*` は将来の制御状態、目標点、誤差などを追加するための予約列です。

## シリアルモニタ例

```text
====================================
GNSS:
Fix: 3D
Satellites: 12
Lat: 35.0000000
Lon: 139.0000000
Speed: 1.23
Course: 180.0
HDOP: 0.90

BNO08X:
Roll: 0.00
Pitch: 0.00
Yaw: 0.00
UpdateHz: 10.0
Accuracy: 3
ReportType: GAME_ROTATION_VECTOR
ResetCount: 0
TimeoutCount: 0
InitFailCount: 0
ReportFailCount: 0
ConsecutiveFail: 0
NextRetryDelayMs: 2000
RecoveryCount: 0

SYSTEM:
Heap: 200000
LoopHz: 1000.0
WiFi Clients: 1

SD:
Mounted: yes
FileReady: yes
File: /LOG000.CSV
Status: logged
Rows: 123
Errors: 0
LastWriteAgeMs: 10
====================================
```

## ディレクトリ構成

```text
.
├── include
│   ├── AppConfig.h
│   ├── GnssManager.h
│   ├── ImuManager.h
│   ├── SystemMonitor.h
│   └── WebMonitor.h
├── src
│   ├── GnssManager.cpp
│   ├── ImuManager.cpp
│   ├── SystemMonitor.cpp
│   ├── WebMonitor.cpp
│   └── main.cpp
├── platformio.ini
└── README.md
```

## 将来拡張予定

- SDカードCSVログ
- 制御側XIAOへのUART送信
- パケット化
- CRC
- ウェイポイント追従
- 異常監視
- IMU冗長比較
- WebSocket
- グラフ表示
- 地図表示
- ウェイポイント表示
- GNSS軌跡表示
- SDログダウンロード
- OTA更新

## シリアルモニタ起動時の注意

PlatformIOのシリアルモニタを開くと、USBシリアルの制御線により
XIAO ESP32S3側だけがリセットされることがあります。この場合、
BNO08Xは通電したままMCUだけが再起動するため、完全な電源投入とは
条件が異なります。

BNO08Xの電源投入直後の動作を確認するときは、先にシリアルモニタを
開いたままにしてからUSBを抜き差ししてください。これでXIAO ESP32S3
SenseとBNO08Xの両方が同時に電源投入されたログを確認できます。

## AP内DNS誘導と接続判定URL

スマホでモバイルデータをONにしたままAPへ接続した場合でもWeb UIへ
到達しやすくするため、AP内だけで動作する簡易DNSサーバーを起動します。
初期設定では接続安定を優先し、全DNSを `192.168.4.1` へ返す
ワイルドカードDNSを有効にしています。この設定ではWeb UIへ到達しやすい
一方で、外部CDNやOpenStreetMapの地図タイルは読み込めない場合があります。

また、スマホやPCがWiFi接続時に使う接続判定URLへ簡単な応答を返します。

```text
/generate_204
/hotspot-detect.html
/connecttest.txt
/ncsi.txt
```

これはAPへ接続した端末向けの簡易キャプティブポータル補助です。
インターネットへの中継や外部公開は行いません。

## Web UI地図表示

Web UIはLeafletとOpenStreetMapの地図タイルを使って、GNSSの現在位置を
地図上に表示します。モバイルデータや別回線でインターネットへ到達
できる端末では、地図背景、現在位置マーカー、GNSS軌跡を確認できます。

XIAOのAPだけに接続していてインターネットへ出られない場合、地図タイルや
Leaflet CDNを読み込めないため地図背景は表示されません。その場合でも
GNSS数値、BNO08X、SD、SYSTEM表示は継続します。
