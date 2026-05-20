#pragma once

#include <Arduino.h>

/*
 * GnssManager
 * 責務:
 * - HardwareSerial(1)からNMEAを非ブロッキングで読み取る。
 * - GGA/RMC/GSA/GSVを解析し、Fix、位置、速度、HDOP、衛星情報を保持する。
 * - GNSS未受信や未Fixでも、呼び出し側の処理を止めない。
 *
 * 公開API:
 * - begin(): GNSS UARTを開始する。
 * - update(): loop()から高頻度に呼び、受信済みバイトだけ処理する。
 * - status(): 最新GNSS状態を読み取る。
 * - satelliteCount()/satelliteAt(): Web UIやログ用に衛星一覧を取得する。
 * - fixLabel(): 表示用Fix文字列を取得する。
 */

struct SatelliteInfo {
  bool active = false;
  bool used = false;
  char system[6] = "GNSS";
  uint16_t id = 0;
  int16_t elevationDeg = -1;
  int16_t azimuthDeg = -1;
  int16_t cnoDbHz = -1;
  uint32_t lastSeenMs = 0;
  uint16_t cycleId = 0;
};

struct GnssStatus {
  bool hasGga = false;
  bool hasRmc = false;
  bool hasLocation = false;
  bool hasAltitude = false;
  bool hasHdop = false;
  bool hasSpeed = false;
  bool hasCourse = false;
  bool nmeaReceived = false;

  uint8_t ggaFixQuality = 0;
  uint8_t gsaFixType = 1;
  uint8_t ggaUsedSatellites = 0;
  uint8_t gsaUsedSatellites = 0;
  uint16_t visibleSatellites = 0;
  uint16_t gsvReportedVisible = 0;

  double latitudeDeg = 0.0;
  double longitudeDeg = 0.0;
  float altitudeM = 0.0f;
  float hdop = 0.0f;
  float pdop = 0.0f;
  float vdop = 0.0f;
  float speedKmh = 0.0f;
  float courseDeg = 0.0f;

  uint32_t charsProcessed = 0;
  uint32_t lastSentenceMs = 0;
};

class GnssManager {
 public:
  struct SatelliteRef {
    char system[6] = "GNSS";
    uint16_t id = 0;
  };

  void begin();
  void update();

  const GnssStatus& status() const { return status_; }
  const char* fixLabel() const;

  size_t satelliteCount() const;
  const SatelliteInfo* satelliteAt(size_t sortedIndex) const;

 private:
  struct GsvSystemState {
    char system[6] = "GNSS";
    uint16_t activeCycle = 0;
    uint16_t reportedVisible = 0;
    uint32_t lastUpdateMs = 0;
  };

  static constexpr size_t kMaxLineLength = 127;
  static constexpr size_t kMaxFields = 24;
  static constexpr size_t kMaxSatellites = 64;
  static constexpr size_t kMaxUsedSatellites = 24;
  static constexpr size_t kMaxGsvSystems = 8;

  void processLine(const char* line);
  void parseGga(char* fields[], size_t fieldCount);
  void parseRmc(char* fields[], size_t fieldCount);
  void parseGsa(const char* talker, char* fields[], size_t fieldCount);
  void parseGsv(const char* talker, char* fields[], size_t fieldCount);
  void refreshSatelliteUsageFlags();
  size_t buildSortedSatelliteIndex(size_t indices[], size_t maxIndices) const;

  bool isSatelliteUsed(const char* system, uint16_t id) const;
  void clearUsedSatellites();
  void addUsedSatellite(const char* system, uint16_t id);
  SatelliteInfo* findSatellite(const char* system, uint16_t id);
  GsvSystemState* getGsvSystemState(const char* system);

  HardwareSerial serial_{1};
  GnssStatus status_;
  SatelliteInfo satellites_[kMaxSatellites];
  SatelliteRef usedSatellites_[kMaxUsedSatellites];
  GsvSystemState gsvStates_[kMaxGsvSystems];
  size_t usedSatelliteCount_ = 0;
  size_t gsvStateCount_ = 0;
  char lineBuffer_[kMaxLineLength + 1] = {};
  size_t lineLength_ = 0;
  uint32_t bootMs_ = 0;
  uint32_t lastGsaSentenceMs_ = 0;
  uint16_t nextGsvCycleId_ = 1;
};
