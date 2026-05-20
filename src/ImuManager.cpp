#include "ImuManager.h"

#include <Wire.h>
#include <math.h>

#include "AppConfig.h"

ImuManager::ImuManager() : bno_(config::kBnoRstPin) {}

namespace {

sh2_SensorId_t orientationReportType() {
  return config::kBnoUseArvrStabilizedReport ? SH2_ARVR_STABILIZED_RV
                                             : SH2_GAME_ROTATION_VECTOR;
}

const char* orientationReportName() {
  return config::kBnoUseArvrStabilizedReport ? "ARVR_STABILIZED_RV"
                                             : "GAME_ROTATION_VECTOR";
}

}  // namespace

void ImuManager::begin() {
  configurePins();
  Serial.printf("[BNO08X] power-on settle %lu ms\n",
                static_cast<unsigned long>(config::kBnoPowerOnSettleMs));
  delay(config::kBnoPowerOnSettleMs);
  initialize("startup");
}

void ImuManager::update() {
  if (status_.initialized) {
    if (bno_.wasReset()) {
      Serial.println("[BNO08X] device reset detected");
      ++status_.resetCount;
      status_.lastResetMs = millis();
      status_.initialized = enableReports();
      if (!status_.initialized) {
        markReportFailure(status_.lastResetMs);
      } else {
        markInitSuccess(status_.lastResetMs);
      }
    }

    for (int i = 0; i < 16; ++i) {
      if (!bno_.getSensorEvent(&sensorValue_)) {
        break;
      }
      handleSensorEvent(sensorValue_);
    }
  }

  const uint32_t now = millis();
  sampleUpdateHz(now);
  recoverIfNeeded();
}

void ImuManager::configurePins() {
  pinMode(config::kBnoRstPin, OUTPUT);
  pinMode(config::kBnoIntPin, INPUT_PULLUP);
  digitalWrite(config::kBnoRstPin, HIGH);
}

void ImuManager::configureI2cBus() {
  Wire.begin(config::kBnoSdaPin, config::kBnoSclPin);
  Wire.setClock(config::kI2cClockHz);
  delay(config::kBnoI2cPostBeginDelayMs);
  Serial.printf("[BNO08X] I2C begin SDA=GPIO%d SCL=GPIO%d clock=%lu\n",
                config::kBnoSdaPin, config::kBnoSclPin,
                static_cast<unsigned long>(config::kI2cClockHz));
}

void ImuManager::hardReset() {
  Serial.printf("[BNO08X] hardware reset LOW %lu ms, settle %lu ms\n",
                static_cast<unsigned long>(config::kBnoResetLowMs),
                static_cast<unsigned long>(config::kBnoPostResetMs));
  digitalWrite(config::kBnoRstPin, LOW);
  delay(config::kBnoResetLowMs);
  digitalWrite(config::kBnoRstPin, HIGH);
  delay(config::kBnoPostResetMs);
}

bool ImuManager::initialize(const char* reason) {
  Serial.printf("[BNO08X] initialize (%s)\n", reason);
  status_.initialized = false;
  status_.hasOrientation = false;
  status_.activeAddress = 0;
  status_.lastEventMs = 0;
  status_.updateHz = 0.0f;
  status_.reportType = orientationReportName();

  hardReset();
  configureI2cBus();

  const uint8_t address = config::kBnoI2cAddress;
  Serial.printf("[BNO08X] try fixed address 0x%02X\n", address);
  if (bno_.begin_I2C(address, &Wire)) {
    status_.activeAddress = address;
    status_.initialized = enableReports();
    Serial.printf("[BNO08X] %s at 0x%02X report=%s\n",
                  status_.initialized ? "ready" : "report setup failed",
                  address, status_.reportType);
    if (status_.initialized) {
      const uint32_t now = millis();
      status_.lastEventMs = now;
      markInitSuccess(now);
      return true;
    }
    markReportFailure(millis());
    return false;
  }

  Serial.println("[BNO08X] not found; system continues without IMU");
  markInitFailure(millis());
  return false;
}

bool ImuManager::enableReports() {
  status_.reportType = orientationReportName();
  const bool orientationOk =
      bno_.enableReport(orientationReportType(), config::kBnoReportIntervalUs);
  const bool gyroOk =
      bno_.enableReport(SH2_GYROSCOPE_CALIBRATED,
                        config::kBnoReportIntervalUs);
  const bool accelOk =
      bno_.enableReport(SH2_ACCELEROMETER, config::kBnoReportIntervalUs);

  Serial.printf("[BNO08X] enable %s: %s\n", status_.reportType,
                orientationOk ? "OK" : "FAILED");
  Serial.printf("[BNO08X] enable GYROSCOPE_CALIBRATED: %s\n",
                gyroOk ? "OK" : "FAILED");
  Serial.printf("[BNO08X] enable ACCELEROMETER: %s\n",
                accelOk ? "OK" : "FAILED");

  return orientationOk && gyroOk && accelOk;
}

void ImuManager::handleSensorEvent(const sh2_SensorValue_t& event) {
  switch (event.sensorId) {
    case SH2_GAME_ROTATION_VECTOR:
      updateEulerFromQuaternion(event.un.gameRotationVector.i,
                                event.un.gameRotationVector.j,
                                event.un.gameRotationVector.k,
                                event.un.gameRotationVector.real);
      break;

    case SH2_ARVR_STABILIZED_RV:
      updateEulerFromQuaternion(event.un.arvrStabilizedRV.i,
                                event.un.arvrStabilizedRV.j,
                                event.un.arvrStabilizedRV.k,
                                event.un.arvrStabilizedRV.real);
      break;

    default:
      return;
  }

  status_.accuracy = event.status;
  status_.hasOrientation = true;
  status_.lastEventMs = millis();
  ++status_.eventCount;
}

void ImuManager::updateEulerFromQuaternion(float i, float j, float k,
                                           float real) {
  // Quaternion axes follow the BNO08X game rotation vector. Output is degrees.
  const float ysqr = j * j;

  const float t0 = +2.0f * (real * i + j * k);
  const float t1 = +1.0f - 2.0f * (i * i + ysqr);
  status_.rollDeg = atan2f(t0, t1) * 180.0f / PI;

  float t2 = +2.0f * (real * j - k * i);
  t2 = t2 > +1.0f ? +1.0f : t2;
  t2 = t2 < -1.0f ? -1.0f : t2;
  status_.pitchDeg = asinf(t2) * 180.0f / PI;

  const float t3 = +2.0f * (real * k + i * j);
  const float t4 = +1.0f - 2.0f * (ysqr + k * k);
  status_.yawDeg = atan2f(t3, t4) * 180.0f / PI;
}

void ImuManager::recoverIfNeeded() {
  const uint32_t now = millis();

  if (status_.initialized) {
    if (status_.lastEventMs != 0 &&
        now - status_.lastEventMs > config::kBnoNoDataTimeoutMs) {
      Serial.println("[BNO08X] timeout; reinitializing");
      ++status_.timeoutCount;
      ++status_.consecutiveFailCount;
      status_.lastTimeoutMs = now;
      status_.nextRetryDelayMs = retryDelayMs();
      lastRetryMs_ = now;
      initialize("sensor timeout");
    }
    return;
  }

  status_.nextRetryDelayMs = retryDelayMs();
  if (now - lastRetryMs_ >= status_.nextRetryDelayMs) {
    lastRetryMs_ = now;
    initialize("retry");
  }
}

void ImuManager::sampleUpdateHz(uint32_t now) {
  if (lastHzSampleMs_ == 0) {
    lastHzSampleMs_ = now;
    eventsAtLastHzSample_ = status_.eventCount;
    return;
  }

  if (now - lastHzSampleMs_ < config::kSystemSampleIntervalMs) {
    return;
  }

  const uint32_t elapsed = now - lastHzSampleMs_;
  const uint32_t events = status_.eventCount - eventsAtLastHzSample_;
  status_.updateHz = elapsed > 0 ? (events * 1000.0f) / elapsed : 0.0f;
  lastHzSampleMs_ = now;
  eventsAtLastHzSample_ = status_.eventCount;
}

uint32_t ImuManager::retryDelayMs() const {
  if (status_.consecutiveFailCount <= 2) {
    return config::kBnoRetryDelayFastMs;
  }
  if (status_.consecutiveFailCount <= 5) {
    return config::kBnoRetryDelayMediumMs;
  }
  if (status_.consecutiveFailCount <= 10) {
    return config::kBnoRetryDelaySlowMs;
  }
  return config::kBnoRetryDelayMaxMs;
}

void ImuManager::markInitSuccess(uint32_t now) {
  if (status_.consecutiveFailCount > 0) {
    ++status_.recoveryCount;
    status_.lastRecoveryMs = now;
  }
  status_.consecutiveFailCount = 0;
  status_.nextRetryDelayMs = retryDelayMs();
}

void ImuManager::markInitFailure(uint32_t now) {
  ++status_.initFailCount;
  ++status_.consecutiveFailCount;
  status_.lastInitFailMs = now;
  status_.nextRetryDelayMs = retryDelayMs();
}

void ImuManager::markReportFailure(uint32_t now) {
  ++status_.reportFailCount;
  ++status_.consecutiveFailCount;
  status_.lastReportFailMs = now;
  status_.nextRetryDelayMs = retryDelayMs();
}
