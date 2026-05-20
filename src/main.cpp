#include <Arduino.h>

#include "AppConfig.h"
#include "GnssManager.h"
#include "ImuManager.h"
#include "SdLogger.h"
#include "SystemMonitor.h"
#include "WebMonitor.h"

namespace {

GnssManager gnss;
ImuManager imu;
SystemMonitor systemMonitor;
SdLogger sdLogger;
WebMonitor webMonitor(gnss, imu, systemMonitor, sdLogger);

uint32_t lastSerialReportMs = 0;

void printBootMessage() {
  Serial.println();
  Serial.println("====================================");
  Serial.println(config::kProjectName);
  Serial.printf("Board: %s\n", config::kBoardName);
  Serial.printf("GNSS UART: MCU RX=GPIO%d MCU TX=GPIO%d @ %lu bps\n",
                config::kGnssRxPin, config::kGnssTxPin,
                static_cast<unsigned long>(config::kGnssBaud));
  Serial.printf("BNO08X: SDA=D4(GPIO%d) SCL=D5(GPIO%d) INT=D3(GPIO%d) RST=D2(GPIO%d)\n",
                config::kBnoSdaPin, config::kBnoSclPin, config::kBnoIntPin,
                config::kBnoRstPin);
  Serial.printf("BNO08X I2C address: 0x%02X, report: %s\n",
                config::kBnoI2cAddress,
                config::kBnoUseArvrStabilizedReport ? "ARVR_STABILIZED_RV"
                                                     : "GAME_ROTATION_VECTOR");
  Serial.printf("SD: SCK=GPIO%d MISO=GPIO%d MOSI=GPIO%d CS=GPIO%d interval=%lu ms\n",
                config::kSdSckPin, config::kSdMisoPin, config::kSdMosiPin,
                config::kSdCsPin,
                static_cast<unsigned long>(config::kSdLogIntervalMs));
  Serial.printf("WiFi AP: %s / %s\n", config::kApSsid, config::kApPassword);
  Serial.println("Web UI: http://192.168.4.1/");
  Serial.println("JSON  : http://192.168.4.1/api/status");
  Serial.println("====================================");
}

void printSerialStatus() {
  const uint32_t now = millis();
  if (now - lastSerialReportMs < config::kSerialReportIntervalMs) {
    return;
  }
  lastSerialReportMs = now;

  const GnssStatus& g = gnss.status();
  const ImuStatus& i = imu.status();
  const SystemStatus& s = systemMonitor.status();
  const SdLogStatus& sd = sdLogger.status();

  Serial.println();
  Serial.println("====================================");
  Serial.println("GNSS:");
  Serial.printf("Fix: %s\n", gnss.fixLabel());
  Serial.printf("Satellites: %u\n", g.visibleSatellites);
  if (g.hasLocation) {
    Serial.printf("Lat: %.7f\n", g.latitudeDeg);
    Serial.printf("Lon: %.7f\n", g.longitudeDeg);
  } else {
    Serial.println("Lat: --");
    Serial.println("Lon: --");
  }
  Serial.printf("Speed: %s\n",
                g.hasSpeed ? String(g.speedKmh, 2).c_str() : "--");
  Serial.printf("Course: %s\n",
                g.hasCourse ? String(g.courseDeg, 1).c_str() : "--");
  Serial.printf("HDOP: %s\n", g.hasHdop ? String(g.hdop, 2).c_str() : "--");

  Serial.println();
  Serial.println("BNO08X:");
  Serial.printf("Roll: %s\n",
                i.hasOrientation ? String(i.rollDeg, 2).c_str() : "--");
  Serial.printf("Pitch: %s\n",
                i.hasOrientation ? String(i.pitchDeg, 2).c_str() : "--");
  Serial.printf("Yaw: %s\n",
                i.hasOrientation ? String(i.yawDeg, 2).c_str() : "--");
  Serial.printf("UpdateHz: %.1f\n", i.updateHz);
  Serial.printf("Accuracy: %u\n", i.accuracy);
  Serial.printf("ReportType: %s\n", i.reportType);
  Serial.printf("ResetCount: %lu\n", static_cast<unsigned long>(i.resetCount));
  Serial.printf("TimeoutCount: %lu\n",
                static_cast<unsigned long>(i.timeoutCount));
  Serial.printf("InitFailCount: %lu\n",
                static_cast<unsigned long>(i.initFailCount));
  Serial.printf("ReportFailCount: %lu\n",
                static_cast<unsigned long>(i.reportFailCount));
  Serial.printf("ConsecutiveFail: %lu\n",
                static_cast<unsigned long>(i.consecutiveFailCount));
  Serial.printf("NextRetryDelayMs: %lu\n",
                static_cast<unsigned long>(i.nextRetryDelayMs));
  Serial.printf("RecoveryCount: %lu\n",
                static_cast<unsigned long>(i.recoveryCount));
  Serial.printf("LastResetAgeMs: %lu\n",
                static_cast<unsigned long>(i.lastResetMs > 0
                                               ? now - i.lastResetMs
                                               : 0));
  Serial.printf("LastTimeoutAgeMs: %lu\n",
                static_cast<unsigned long>(i.lastTimeoutMs > 0
                                               ? now - i.lastTimeoutMs
                                               : 0));
  Serial.printf("LastRecoveryAgeMs: %lu\n",
                static_cast<unsigned long>(i.lastRecoveryMs > 0
                                               ? now - i.lastRecoveryMs
                                               : 0));

  Serial.println();
  Serial.println("SD:");
  Serial.printf("Mounted: %s\n", sd.mounted ? "yes" : "no");
  Serial.printf("FileReady: %s\n", sd.fileReady ? "yes" : "no");
  Serial.printf("File: %s\n", sd.filePath);
  Serial.printf("Status: %s\n", sd.lastStatus);
  Serial.printf("Rows: %lu\n", static_cast<unsigned long>(sd.writeCount));
  Serial.printf("Errors: %lu\n", static_cast<unsigned long>(sd.writeErrorCount));
  Serial.printf("LastWriteAgeMs: %lu\n",
                static_cast<unsigned long>(sd.lastWriteMs > 0
                                               ? now - sd.lastWriteMs
                                               : 0));

  Serial.println();
  Serial.println("SYSTEM:");
  Serial.printf("Heap: %lu\n", static_cast<unsigned long>(s.freeHeap));
  Serial.printf("LoopHz: %.1f\n", s.loopHz);
  Serial.printf("WiFi Clients: %u\n", s.wifiClients);
  Serial.println("====================================");
}

}  // namespace

void setup() {
  Serial.begin(config::kUsbSerialBaud);

  printBootMessage();

  systemMonitor.begin();
  gnss.begin();
  imu.begin();
  sdLogger.begin();
  webMonitor.begin();
}

void loop() {
  systemMonitor.markLoop();

  gnss.update();
  imu.update();
  systemMonitor.update();
  sdLogger.update(gnss.status(), gnss.fixLabel(), imu.status(),
                  systemMonitor.status());
  printSerialStatus();
}
