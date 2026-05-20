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
| GNSS MCU RX | GPIO1 |
| GNSS MCU TX | GPIO0 |

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
```

GNSSモジュールの電源電圧は使用モジュールの仕様に合わせてください。

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
- SYSTEM: millis、free heap、WiFi接続数、loop周期
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
    "updateHz": 10.0
  },
  "system": {
    "millis": 123456,
    "freeHeap": 200000,
    "wifiClients": 1,
    "loopHz": 1000.0
  },
  "satellites": []
}
```

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

SYSTEM:
Heap: 200000
LoopHz: 1000.0
WiFi Clients: 1
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
