#pragma once

#include <Arduino.h>

/*
 * SystemMonitor
 * 責務:
 * - loop周期、ヒープ、WiFi接続数など、センサ以外の状態を集約する。
 * - 将来、SDログ、UART送信、異常監視の状態を追加しやすい場所にする。
 *
 * 公開API:
 * - begin(): 起動時刻を記録する。
 * - markLoop(): loop()の先頭または末尾で呼び、周期計測に使う。
 * - update(): 1秒程度の周期でLoopHzなどを更新する。
 * - status(): 最新システム状態を読み取る。
 */

struct SystemStatus {
  uint32_t millisNow = 0;
  uint32_t freeHeap = 0;
  uint8_t wifiClients = 0;
  float loopHz = 0.0f;
};

class SystemMonitor {
 public:
  void begin();
  void markLoop();
  void update();
  const SystemStatus& status() const { return status_; }

 private:
  SystemStatus status_;
  uint32_t lastSampleMs_ = 0;
  uint32_t loopCount_ = 0;
  uint32_t loopCountAtLastSample_ = 0;
};
