#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SPI.h>

#include "GnssManager.h"
#include "ImuManager.h"
#include "SystemMonitor.h"

/*
 * SdLogger
 * Responsibility:
 * - Mount the XIAO ESP32S3 Sense microSD card.
 * - Create a new versioned CSV log file without overwriting old logs.
 * - Append GNSS, BNO08X, BNO diagnostics, and system status periodically.
 * - Keep logging failures visible without stopping sensor/web operation.
 *
 * Public API:
 * - begin(): Mount SD and prepare the next CSV file.
 * - update(): Called from loop(); writes one row when the configured interval elapses.
 * - status(): Read SD/log state for serial, web, and JSON API.
 */

struct SdLogStatus {
  bool mounted = false;
  bool fileReady = false;
  int csPin = -1;
  char filePath[32] = "(none)";
  char lastStatus[32] = "not started";
  uint32_t writeCount = 0;
  uint32_t writeErrorCount = 0;
  uint32_t lastWriteMs = 0;
  uint32_t lastErrorMs = 0;
};

class SdLogger {
 public:
  void begin();
  void update(const GnssStatus& gnss, const char* fixLabel,
              const ImuStatus& imu, const SystemStatus& system);
  const SdLogStatus& status() const { return status_; }

 private:
  bool chooseNextLogFilePath(char* path, size_t pathSize);
  bool prepareLogFile();
  void writeHeader();
  bool appendRow(const GnssStatus& gnss, const char* fixLabel,
                 const ImuStatus& imu, const SystemStatus& system);
  void setStatusText(const char* text);
  void recordWriteError(const char* text);

  SPIClass sdSpi_{FSPI};
  File logFile_;
  SdLogStatus status_;
  uint32_t lastLogMs_ = 0;
  uint32_t sequence_ = 0;
};
