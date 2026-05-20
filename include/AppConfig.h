#pragma once

#include <Arduino.h>

/*
 * AppConfig
 * 責務:
 * - Sense側マイコン全体で共有するピン、通信速度、周期、WiFi設定を集約する。
 * - 将来、SDログや制御側UARTを追加するときは、まずこのファイルに設定を追加する。
 *
 * 公開API:
 * - config名前空間内の定数を各モジュールから参照する。
 */
namespace config {

static constexpr const char* kProjectName =
    "XIAO ESP32S3 Sense GNSS BNO08X Web Monitor";
static constexpr const char* kBoardName = "Seeed Studio XIAO ESP32S3 Sense";

static constexpr uint32_t kUsbSerialBaud = 115200;

// GNSS UART wiring reused from the working standalone GNSS monitor.
static constexpr uint32_t kGnssBaud = 115200;
static constexpr int kGnssRxPin = 1;
static constexpr int kGnssTxPin = 0;

// BNO08X wiring follows the working standalone BNO08X project.
// On XIAO ESP32S3 in PlatformIO: D4=GPIO5, D5=GPIO6, D3=GPIO4, D2=GPIO3.
static constexpr int kBnoSdaPin = D4;
static constexpr int kBnoSclPin = D5;
static constexpr int kBnoIntPin = D3;
static constexpr int kBnoRstPin = D2;

static constexpr uint8_t kBnoI2cAddress = 0x4A;
static constexpr uint32_t kI2cClockHz = 100000;
static constexpr uint32_t kBnoPowerOnSettleMs = 1500;
static constexpr uint32_t kBnoResetLowMs = 10;
static constexpr uint32_t kBnoPostResetMs = 300;
static constexpr uint32_t kBnoI2cPostBeginDelayMs = 10;
static constexpr uint32_t kBnoReportIntervalUs = 100000;
static constexpr bool kBnoUseArvrStabilizedReport = false;
static constexpr uint32_t kBnoNoDataTimeoutMs = 5000;
static constexpr uint32_t kBnoRetryDelayFastMs = 2000;
static constexpr uint32_t kBnoRetryDelayMediumMs = 5000;
static constexpr uint32_t kBnoRetryDelaySlowMs = 10000;
static constexpr uint32_t kBnoRetryDelayMaxMs = 30000;

// XIAO ESP32S3 Sense microSD SPI wiring.
// GPIO3 is already used by BNO08X RST, so the old SD fallback CS=GPIO3 is not used here.
static constexpr int kSdSckPin = 7;
static constexpr int kSdMisoPin = 8;
static constexpr int kSdMosiPin = 9;
static constexpr int kSdCsPin = 21;
static constexpr uint32_t kSdLogIntervalMs = 1000;

static constexpr const char* kApSsid = "XIAO_BOAT_MONITOR";
static constexpr const char* kApPassword = "12345678";

static constexpr uint32_t kSerialReportIntervalMs = 1000;
static constexpr uint32_t kWebRefreshIntervalMs = 1000;
static constexpr uint32_t kSystemSampleIntervalMs = 1000;
static constexpr uint32_t kSatelliteKeepaliveMs = 5000;

}  // namespace config
