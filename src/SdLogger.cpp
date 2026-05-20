#include "SdLogger.h"

#include <SD.h>

#include "AppConfig.h"

namespace {

void appendCsvString(String& row, const char* text) {
  bool quote = false;
  for (const char* p = text; p != nullptr && *p != '\0'; ++p) {
    if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
      quote = true;
      break;
    }
  }

  if (!quote) {
    row += text;
    return;
  }

  row += '"';
  for (const char* p = text; p != nullptr && *p != '\0'; ++p) {
    if (*p == '"') {
      row += "\"\"";
    } else if (*p != '\r') {
      row += *p;
    }
  }
  row += '"';
}

}  // namespace

void SdLogger::begin() {
  setStatusText("mounting");

  sdSpi_.begin(config::kSdSckPin, config::kSdMisoPin, config::kSdMosiPin,
               config::kSdCsPin);

  if (!SD.begin(config::kSdCsPin, sdSpi_)) {
    status_.mounted = false;
    status_.fileReady = false;
    status_.csPin = -1;
    recordWriteError("mount failed");
    Serial.println("[SD] mount failed; logging disabled");
    return;
  }

  status_.mounted = true;
  status_.csPin = config::kSdCsPin;
  Serial.printf("[SD] mounted CS=GPIO%d\n", status_.csPin);

  if (!prepareLogFile()) {
    return;
  }

  setStatusText("ready");
}

void SdLogger::update(const GnssStatus& gnss, const char* fixLabel,
                      const ImuStatus& imu, const SystemStatus& system) {
  const uint32_t now = millis();
  if (now - lastLogMs_ < config::kSdLogIntervalMs) {
    return;
  }
  lastLogMs_ = now;

  if (!status_.mounted || !status_.fileReady) {
    return;
  }

  if (!appendRow(gnss, fixLabel, imu, system)) {
    recordWriteError("write failed");
    return;
  }

  ++status_.writeCount;
  status_.lastWriteMs = now;
  setStatusText("logged");
}

bool SdLogger::chooseNextLogFilePath(char* path, size_t pathSize) {
  for (uint16_t index = 0; index <= 999; ++index) {
    snprintf(path, pathSize, "/LOG%03u.CSV", index);
    if (!SD.exists(path)) {
      return true;
    }
  }

  snprintf(path, pathSize, "/LOGOVF.CSV");
  return !SD.exists(path);
}

bool SdLogger::prepareLogFile() {
  if (!chooseNextLogFilePath(status_.filePath, sizeof(status_.filePath))) {
    recordWriteError("no free name");
    return false;
  }

  logFile_ = SD.open(status_.filePath, FILE_WRITE);
  if (!logFile_) {
    status_.fileReady = false;
    recordWriteError("open failed");
    return false;
  }

  status_.fileReady = true;
  writeHeader();
  logFile_.flush();
  Serial.printf("[SD] log file: %s\n", status_.filePath);
  return true;
}

void SdLogger::writeHeader() {
  logFile_.println("# log_format_version,2");
  logFile_.println("# project,XIAO ESP32S3 Sense GNSS BNO08X Web Monitor");
  logFile_.println("# time/system,gnss,bno08x,imu_diag,future_control");
  logFile_.println(
      "millis,log_seq,"
      "gnss_fix,gnss_sats,gnss_used_sats,gnss_has_location,"
      "gnss_lat,gnss_lon,gnss_has_speed,gnss_speed_kmh,"
      "gnss_has_course,gnss_course_deg,gnss_has_hdop,gnss_hdop,"
      "bno_initialized,bno_has_orientation,bno_roll_deg,bno_pitch_deg,"
      "bno_yaw_deg,bno_update_hz,"
      "bno_reset_count,bno_timeout_count,bno_init_fail_count,"
      "bno_report_fail_count,bno_accuracy_status,bno_report_type,"
      "bno_consecutive_fail,bno_next_retry_delay_ms,bno_recovery_count,"
      "system_heap,system_loop_hz,system_wifi_clients,"
      "control_state,control_target_lat,control_target_lon,control_error");
}

bool SdLogger::appendRow(const GnssStatus& gnss, const char* fixLabel,
                         const ImuStatus& imu, const SystemStatus& system) {
  ++sequence_;

  String row;
  row.reserve(512);
  row += String(millis());
  row += ",";
  row += String(sequence_);
  row += ",";
  appendCsvString(row, fixLabel);
  row += ",";
  row += String(gnss.visibleSatellites);
  row += ",";
  row += String(gnss.gsaUsedSatellites > 0 ? gnss.gsaUsedSatellites
                                           : gnss.ggaUsedSatellites);
  row += ",";
  row += gnss.hasLocation ? "1" : "0";
  row += ",";
  row += gnss.hasLocation ? String(gnss.latitudeDeg, 7) : "";
  row += ",";
  row += gnss.hasLocation ? String(gnss.longitudeDeg, 7) : "";
  row += ",";
  row += gnss.hasSpeed ? "1" : "0";
  row += ",";
  row += gnss.hasSpeed ? String(gnss.speedKmh, 2) : "";
  row += ",";
  row += gnss.hasCourse ? "1" : "0";
  row += ",";
  row += gnss.hasCourse ? String(gnss.courseDeg, 1) : "";
  row += ",";
  row += gnss.hasHdop ? "1" : "0";
  row += ",";
  row += gnss.hasHdop ? String(gnss.hdop, 2) : "";
  row += ",";
  row += imu.initialized ? "1" : "0";
  row += ",";
  row += imu.hasOrientation ? "1" : "0";
  row += ",";
  row += imu.hasOrientation ? String(imu.rollDeg, 2) : "";
  row += ",";
  row += imu.hasOrientation ? String(imu.pitchDeg, 2) : "";
  row += ",";
  row += imu.hasOrientation ? String(imu.yawDeg, 2) : "";
  row += ",";
  row += String(imu.updateHz, 1);
  row += ",";
  row += String(imu.resetCount);
  row += ",";
  row += String(imu.timeoutCount);
  row += ",";
  row += String(imu.initFailCount);
  row += ",";
  row += String(imu.reportFailCount);
  row += ",";
  row += String(imu.accuracy);
  row += ",";
  appendCsvString(row, imu.reportType);
  row += ",";
  row += String(imu.consecutiveFailCount);
  row += ",";
  row += String(imu.nextRetryDelayMs);
  row += ",";
  row += String(imu.recoveryCount);
  row += ",";
  row += String(system.freeHeap);
  row += ",";
  row += String(system.loopHz, 1);
  row += ",";
  row += String(system.wifiClients);
  row += ",";
  row += ",,,";  // future control fields

  if (!logFile_.println(row)) {
    return false;
  }

  logFile_.flush();
  return true;
}

void SdLogger::setStatusText(const char* text) {
  strncpy(status_.lastStatus, text, sizeof(status_.lastStatus) - 1);
  status_.lastStatus[sizeof(status_.lastStatus) - 1] = '\0';
}

void SdLogger::recordWriteError(const char* text) {
  ++status_.writeErrorCount;
  status_.lastErrorMs = millis();
  setStatusText(text);
}
