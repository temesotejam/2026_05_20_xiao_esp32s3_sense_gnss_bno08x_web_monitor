#include "SystemMonitor.h"

#include <WiFi.h>

#include "AppConfig.h"

void SystemMonitor::begin() {
  lastSampleMs_ = millis();
  status_.millisNow = lastSampleMs_;
  status_.freeHeap = ESP.getFreeHeap();
}

void SystemMonitor::markLoop() {
  ++loopCount_;
}

void SystemMonitor::update() {
  const uint32_t now = millis();
  status_.millisNow = now;
  status_.freeHeap = ESP.getFreeHeap();
  status_.wifiClients = WiFi.softAPgetStationNum();

  if (now - lastSampleMs_ < config::kSystemSampleIntervalMs) {
    return;
  }

  const uint32_t elapsed = now - lastSampleMs_;
  const uint32_t loops = loopCount_ - loopCountAtLastSample_;
  status_.loopHz = elapsed > 0 ? (loops * 1000.0f) / elapsed : 0.0f;
  loopCountAtLastSample_ = loopCount_;
  lastSampleMs_ = now;
}
