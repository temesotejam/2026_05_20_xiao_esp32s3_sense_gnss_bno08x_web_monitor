#include "ImuManager.h"

#include <Wire.h>
#include <math.h>

#include "AppConfig.h"

ImuManager::ImuManager() : bno_(config::kBnoRstPin) {}

void ImuManager::begin() {
  configurePins();
  initialize("startup");
}

void ImuManager::update() {
  if (status_.initialized) {
    if (bno_.wasReset()) {
      Serial.println("[BNO08X] device reset detected");
      status_.initialized = enableReports();
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
}

void ImuManager::hardReset() {
  digitalWrite(config::kBnoRstPin, LOW);
  delay(10);
  digitalWrite(config::kBnoRstPin, HIGH);
  delay(300);
}

bool ImuManager::initialize(const char* reason) {
  Serial.printf("[BNO08X] initialize (%s)\n", reason);
  status_.initialized = false;
  status_.hasOrientation = false;
  status_.activeAddress = 0;
  status_.lastEventMs = 0;
  status_.updateHz = 0.0f;

  hardReset();
  configureI2cBus();

  const uint8_t candidateAddresses[] = {
      config::kBnoAddressPrimary,
      config::kBnoAddressAlternate,
  };

  for (uint8_t address : candidateAddresses) {
    Serial.printf("[BNO08X] try 0x%02X\n", address);
    if (bno_.begin_I2C(address, &Wire)) {
      status_.activeAddress = address;
      status_.initialized = enableReports();
      Serial.printf("[BNO08X] %s at 0x%02X\n",
                    status_.initialized ? "ready" : "report setup failed",
                    address);
      if (status_.initialized) {
        status_.lastEventMs = millis();
        return true;
      }
    }
  }

  Serial.println("[BNO08X] not found; system continues without IMU");
  return false;
}

bool ImuManager::enableReports() {
  bool ok = true;
  ok &= bno_.enableReport(SH2_GAME_ROTATION_VECTOR, config::kBnoReportIntervalUs);
  return ok;
}

void ImuManager::handleSensorEvent(const sh2_SensorValue_t& event) {
  if (event.sensorId != SH2_GAME_ROTATION_VECTOR) {
    return;
  }

  updateEulerFromQuaternion(event.un.gameRotationVector.i,
                            event.un.gameRotationVector.j,
                            event.un.gameRotationVector.k,
                            event.un.gameRotationVector.real);
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
      lastRetryMs_ = now;
      initialize("sensor timeout");
    }
    return;
  }

  if (now - lastRetryMs_ >= config::kBnoRetryIntervalMs) {
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
