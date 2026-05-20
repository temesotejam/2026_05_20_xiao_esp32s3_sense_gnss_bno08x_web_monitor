#pragma once

#include <Arduino.h>
#include <Adafruit_BNO08x.h>

/*
 * ImuManager
 * 責務:
 * - GPIO4/GPIO5 I2C上のBNO08Xを初期化し、姿勢データを取得する。
 * - IMU未接続、初期化失敗、途中リセット時もシステム全体を止めずに復旧を試みる。
 * - Web/API/シリアル表示向けにRoll/Pitch/Yawと更新Hzを保持する。
 *
 * 公開API:
 * - begin(): I2CとBNO08X初期化を行う。
 * - update(): loop()から高頻度に呼び、受信済みイベントだけ処理する。
 * - status(): 最新IMU状態を読み取る。
 */

struct ImuStatus {
  bool initialized = false;
  bool hasOrientation = false;
  uint8_t activeAddress = 0;
  float rollDeg = 0.0f;
  float pitchDeg = 0.0f;
  float yawDeg = 0.0f;
  float updateHz = 0.0f;
  uint32_t lastEventMs = 0;
  uint32_t eventCount = 0;
};

class ImuManager {
 public:
  ImuManager();
  void begin();
  void update();
  const ImuStatus& status() const { return status_; }

 private:
  void configurePins();
  void configureI2cBus();
  void hardReset();
  bool initialize(const char* reason);
  bool enableReports();
  void handleSensorEvent(const sh2_SensorValue_t& event);
  void updateEulerFromQuaternion(float i, float j, float k, float real);
  void recoverIfNeeded();
  void sampleUpdateHz(uint32_t now);

  Adafruit_BNO08x bno_;
  sh2_SensorValue_t sensorValue_ = {};
  ImuStatus status_;
  uint32_t lastRetryMs_ = 0;
  uint32_t lastHzSampleMs_ = 0;
  uint32_t eventsAtLastHzSample_ = 0;
};
