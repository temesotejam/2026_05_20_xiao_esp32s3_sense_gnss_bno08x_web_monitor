#include <Arduino.h>

#include "AppConfig.h"
#include "GnssManager.h"
#include "ImuManager.h"
#include "SystemMonitor.h"
#include "WebMonitor.h"

namespace {

GnssManager gnss;
ImuManager imu;
SystemMonitor systemMonitor;
WebMonitor webMonitor(gnss, imu, systemMonitor);

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
  webMonitor.begin();
}

void loop() {
  systemMonitor.markLoop();

  gnss.update();
  imu.update();
  systemMonitor.update();
  printSerialStatus();
}
