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

// GNSS UART wiring requested for this integrated Sense-side project.
static constexpr uint32_t kGnssBaud = 115200;
static constexpr int kGnssRxPin = 0;
static constexpr int kGnssTxPin = 1;

// BNO08X wiring. GPIO numbers are used directly to avoid pin-label ambiguity.
static constexpr int kBnoSdaPin = 4;
static constexpr int kBnoSclPin = 5;
static constexpr int kBnoIntPin = 3;
static constexpr int kBnoRstPin = 2;

static constexpr uint8_t kBnoAddressPrimary = 0x4A;
static constexpr uint8_t kBnoAddressAlternate = 0x4B;
static constexpr uint32_t kI2cClockHz = 100000;
static constexpr uint32_t kBnoReportIntervalUs = 100000;

static constexpr const char* kApSsid = "XIAO_BOAT_MONITOR";
static constexpr const char* kApPassword = "12345678";

static constexpr uint32_t kSerialReportIntervalMs = 1000;
static constexpr uint32_t kWebRefreshIntervalMs = 1000;
static constexpr uint32_t kSystemSampleIntervalMs = 1000;
static constexpr uint32_t kBnoNoDataTimeoutMs = 3000;
static constexpr uint32_t kBnoRetryIntervalMs = 2000;
static constexpr uint32_t kSatelliteKeepaliveMs = 5000;

}  // namespace config
