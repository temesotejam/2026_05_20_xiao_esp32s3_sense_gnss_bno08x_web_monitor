#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "GnssManager.h"
#include "ImuManager.h"
#include "SystemMonitor.h"

/*
 * WebMonitor
 * 責務:
 * - XIAO ESP32S3 SenseをWiFiアクセスポイントとして起動する。
 * - 状態監視用HTMLページと /api/status JSON APIを提供する。
 * - 将来WebSocket、グラフ、地図、SDログダウンロードを追加しやすい境界にする。
 *
 * 公開API:
 * - begin(): APとAsyncWebServerを開始する。
 * - jsonStatus(): 現在状態のJSON文字列を生成する。
 */

class WebMonitor {
 public:
  WebMonitor(GnssManager& gnss, ImuManager& imu, SystemMonitor& system);

  void begin();
  String jsonStatus() const;

 private:
  String htmlPage() const;

  GnssManager& gnss_;
  ImuManager& imu_;
  SystemMonitor& system_;
  AsyncWebServer server_{80};
};
