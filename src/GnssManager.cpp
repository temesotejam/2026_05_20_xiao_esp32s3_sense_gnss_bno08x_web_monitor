#include "GnssManager.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "AppConfig.h"

namespace {

uint8_t hexToNibble(char c) {
  if (c >= '0' && c <= '9') {
    return static_cast<uint8_t>(c - '0');
  }
  if (c >= 'A' && c <= 'F') {
    return static_cast<uint8_t>(c - 'A' + 10);
  }
  if (c >= 'a' && c <= 'f') {
    return static_cast<uint8_t>(c - 'a' + 10);
  }
  return 0xFF;
}

bool isFieldPresent(const char* text) {
  return text != nullptr && text[0] != '\0';
}

float parseFloatOr(const char* text, float fallback) {
  if (!isFieldPresent(text)) {
    return fallback;
  }
  return static_cast<float>(strtod(text, nullptr));
}

uint16_t parseUint16Or(const char* text, uint16_t fallback) {
  if (!isFieldPresent(text)) {
    return fallback;
  }
  return static_cast<uint16_t>(strtoul(text, nullptr, 10));
}

double parseLatLon(const char* field, const char* hemi) {
  if (!isFieldPresent(field) || !isFieldPresent(hemi)) {
    return 0.0;
  }

  const double raw = strtod(field, nullptr);
  const double degrees = floor(raw / 100.0);
  const double minutes = raw - (degrees * 100.0);
  double value = degrees + (minutes / 60.0);

  if (hemi[0] == 'S' || hemi[0] == 'W') {
    value = -value;
  }
  return value;
}

bool verifyChecksum(const char* line) {
  const char* star = strchr(line, '*');
  if (line[0] != '$' || star == nullptr || (star - line) < 1) {
    return false;
  }
  if (star[1] == '\0' || star[2] == '\0') {
    return false;
  }

  uint8_t checksum = 0;
  for (const char* p = line + 1; p < star; ++p) {
    checksum ^= static_cast<uint8_t>(*p);
  }

  const uint8_t high = hexToNibble(star[1]);
  const uint8_t low = hexToNibble(star[2]);
  if (high == 0xFF || low == 0xFF) {
    return false;
  }

  return checksum == static_cast<uint8_t>((high << 4) | low);
}

size_t splitFields(char* payload, char* fields[], size_t maxFields) {
  size_t count = 0;
  char* cursor = payload;

  while (count < maxFields && cursor != nullptr) {
    fields[count++] = cursor;
    char* comma = strchr(cursor, ',');
    if (comma == nullptr) {
      break;
    }
    *comma = '\0';
    cursor = comma + 1;
  }

  return count;
}

const char* inferSystemFromId(uint16_t id) {
  if (id >= 193 && id <= 199) {
    return "QZSS";
  }
  if (id >= 65 && id <= 96) {
    return "GLO";
  }
  if (id >= 201 && id <= 237) {
    return "BDS";
  }
  if (id >= 301 && id <= 336) {
    return "GAL";
  }
  if (id >= 1 && id <= 32) {
    return "GPS";
  }
  return "GNSS";
}

const char* talkerToSystem(const char* talker, uint16_t satId) {
  if (strcmp(talker, "GP") == 0) {
    return "GPS";
  }
  if (strcmp(talker, "GL") == 0) {
    return "GLO";
  }
  if (strcmp(talker, "GA") == 0) {
    return "GAL";
  }
  if (strcmp(talker, "GB") == 0 || strcmp(talker, "BD") == 0) {
    return "BDS";
  }
  if (strcmp(talker, "GQ") == 0) {
    return "QZSS";
  }
  return inferSystemFromId(satId);
}

const char* gsaSystemIdToSystem(uint16_t systemId) {
  switch (systemId) {
    case 1:
      return "GPS";
    case 2:
      return "GLO";
    case 3:
      return "GAL";
    case 4:
      return "BDS";
    case 5:
      return "QZSS";
    default:
      return "GNSS";
  }
}

bool satelliteRefEquals(const GnssManager::SatelliteRef& ref, const char* system,
                        uint16_t id) {
  return ref.id == id && strcmp(ref.system, system) == 0;
}

}  // namespace

void GnssManager::begin() {
  serial_.begin(config::kGnssBaud, SERIAL_8N1, config::kGnssRxPin,
                config::kGnssTxPin);
  bootMs_ = millis();
  status_.lastSentenceMs = bootMs_;
}

void GnssManager::update() {
  while (serial_.available() > 0) {
    const int raw = serial_.read();
    if (raw < 0) {
      break;
    }

    ++status_.charsProcessed;
    const char c = static_cast<char>(raw);

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      if (lineLength_ > 0) {
        lineBuffer_[lineLength_] = '\0';
        processLine(lineBuffer_);
        lineLength_ = 0;
      }
      continue;
    }

    if (c == '$') {
      lineLength_ = 0;
    }

    if (lineLength_ < kMaxLineLength) {
      lineBuffer_[lineLength_++] = c;
    } else {
      lineLength_ = 0;
    }
  }

  refreshSatelliteUsageFlags();
}

const char* GnssManager::fixLabel() const {
  if (status_.ggaFixQuality > 0 || status_.gsaFixType >= 2) {
    if (status_.gsaFixType >= 3) {
      return "3D";
    }
    if (status_.gsaFixType == 2) {
      return "2D";
    }
    return "Fix";
  }
  return "No Fix";
}

size_t GnssManager::satelliteCount() const {
  size_t indices[kMaxSatellites] = {};
  return buildSortedSatelliteIndex(indices, kMaxSatellites);
}

const SatelliteInfo* GnssManager::satelliteAt(size_t sortedIndex) const {
  size_t indices[kMaxSatellites] = {};
  const size_t count = buildSortedSatelliteIndex(indices, kMaxSatellites);
  if (sortedIndex >= count) {
    return nullptr;
  }
  return &satellites_[indices[sortedIndex]];
}

void GnssManager::processLine(const char* line) {
  if (line[0] != '$' || !verifyChecksum(line)) {
    return;
  }

  char payload[kMaxLineLength + 1] = {};
  const char* star = strchr(line, '*');
  if (star == nullptr) {
    return;
  }

  const size_t payloadLength = static_cast<size_t>(star - (line + 1));
  if (payloadLength == 0 || payloadLength > kMaxLineLength) {
    return;
  }

  memcpy(payload, line + 1, payloadLength);
  payload[payloadLength] = '\0';

  char* fields[kMaxFields] = {};
  const size_t fieldCount = splitFields(payload, fields, kMaxFields);
  if (fieldCount == 0 || strlen(fields[0]) < 5) {
    return;
  }

  char talker[3] = {fields[0][0], fields[0][1], '\0'};
  const char* sentence = fields[0] + 2;

  status_.nmeaReceived = true;
  status_.lastSentenceMs = millis();

  if (strcmp(sentence, "GGA") == 0) {
    parseGga(fields, fieldCount);
  } else if (strcmp(sentence, "GSA") == 0) {
    parseGsa(talker, fields, fieldCount);
  } else if (strcmp(sentence, "GSV") == 0) {
    parseGsv(talker, fields, fieldCount);
  } else if (strcmp(sentence, "RMC") == 0) {
    parseRmc(fields, fieldCount);
  }
}

void GnssManager::parseGga(char* fields[], size_t fieldCount) {
  if (fieldCount < 10) {
    return;
  }

  status_.hasGga = true;
  status_.ggaFixQuality = static_cast<uint8_t>(parseUint16Or(fields[6], 0));
  status_.ggaUsedSatellites = static_cast<uint8_t>(parseUint16Or(fields[7], 0));
  status_.hdop = parseFloatOr(fields[8], status_.hdop);
  status_.hasHdop = isFieldPresent(fields[8]);
  status_.altitudeM = parseFloatOr(fields[9], status_.altitudeM);
  status_.hasAltitude = isFieldPresent(fields[9]);

  if (status_.ggaFixQuality > 0 && isFieldPresent(fields[2]) &&
      isFieldPresent(fields[3]) && isFieldPresent(fields[4]) &&
      isFieldPresent(fields[5])) {
    status_.latitudeDeg = parseLatLon(fields[2], fields[3]);
    status_.longitudeDeg = parseLatLon(fields[4], fields[5]);
    status_.hasLocation = true;
  }
}

void GnssManager::parseRmc(char* fields[], size_t fieldCount) {
  if (fieldCount < 9) {
    return;
  }

  status_.hasRmc = true;
  const bool active = isFieldPresent(fields[2]) && fields[2][0] == 'A';

  if (active && isFieldPresent(fields[3]) && isFieldPresent(fields[4]) &&
      isFieldPresent(fields[5]) && isFieldPresent(fields[6])) {
    status_.latitudeDeg = parseLatLon(fields[3], fields[4]);
    status_.longitudeDeg = parseLatLon(fields[5], fields[6]);
    status_.hasLocation = true;
  }

  if (active && isFieldPresent(fields[7])) {
    status_.speedKmh = parseFloatOr(fields[7], 0.0f) * 1.852f;
    status_.hasSpeed = true;
  }

  if (active && isFieldPresent(fields[8])) {
    status_.courseDeg = parseFloatOr(fields[8], status_.courseDeg);
    status_.hasCourse = true;
  }
}

void GnssManager::parseGsa(const char* talker, char* fields[],
                           size_t fieldCount) {
  if (fieldCount < 17) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastGsaSentenceMs_ > 1500) {
    clearUsedSatellites();
  }
  lastGsaSentenceMs_ = now;

  status_.gsaFixType =
      static_cast<uint8_t>(parseUint16Or(fields[2], status_.gsaFixType));

  const uint16_t systemId =
      fieldCount > 18 ? parseUint16Or(fields[18], 0) : 0;
  const char* gsaSystem =
      systemId != 0 ? gsaSystemIdToSystem(systemId) : talkerToSystem(talker, 0);

  for (size_t i = 3; i <= 14 && i < fieldCount; ++i) {
    const uint16_t satId = parseUint16Or(fields[i], 0);
    if (satId != 0) {
      addUsedSatellite(gsaSystem, satId);
    }
  }

  status_.pdop = parseFloatOr(fields[15], status_.pdop);
  status_.hdop = parseFloatOr(fields[16], status_.hdop);
  status_.vdop = (fieldCount > 17) ? parseFloatOr(fields[17], status_.vdop)
                                  : status_.vdop;
  status_.hasHdop = status_.hasHdop || isFieldPresent(fields[16]);

  refreshSatelliteUsageFlags();
}

void GnssManager::parseGsv(const char* talker, char* fields[],
                           size_t fieldCount) {
  if (fieldCount < 4) {
    return;
  }

  const uint16_t totalMessages = parseUint16Or(fields[1], 0);
  const uint16_t messageNumber = parseUint16Or(fields[2], 0);
  const uint16_t reportedVisible = parseUint16Or(fields[3], 0);
  const char* stateSystem = talkerToSystem(talker, 0);
  GsvSystemState* state = getGsvSystemState(stateSystem);
  if (state == nullptr) {
    return;
  }

  if (messageNumber <= 1 || state->activeCycle == 0) {
    state->activeCycle = nextGsvCycleId_++;
  }
  state->reportedVisible = reportedVisible;
  state->lastUpdateMs = millis();

  for (size_t i = 4; i + 3 < fieldCount; i += 4) {
    const uint16_t satId = parseUint16Or(fields[i], 0);
    if (satId == 0) {
      continue;
    }

    const char* system = talkerToSystem(talker, satId);
    SatelliteInfo* sat = findSatellite(system, satId);
    if (sat == nullptr) {
      continue;
    }

    sat->active = true;
    sat->id = satId;
    sat->elevationDeg = static_cast<int16_t>(parseUint16Or(fields[i + 1], 0));
    sat->azimuthDeg = static_cast<int16_t>(parseUint16Or(fields[i + 2], 0));
    sat->cnoDbHz = isFieldPresent(fields[i + 3])
                       ? static_cast<int16_t>(parseUint16Or(fields[i + 3], 0))
                       : -1;
    sat->lastSeenMs = millis();
    sat->cycleId = state->activeCycle;
    sat->used = isSatelliteUsed(system, satId);
  }

  if (messageNumber == totalMessages) {
    for (size_t i = 0; i < kMaxSatellites; ++i) {
      if (satellites_[i].active &&
          strcmp(satellites_[i].system, stateSystem) == 0 &&
          satellites_[i].cycleId != state->activeCycle) {
        satellites_[i].active = false;
        satellites_[i].used = false;
      }
    }
  }

  refreshSatelliteUsageFlags();
}

void GnssManager::refreshSatelliteUsageFlags() {
  const uint32_t now = millis();
  uint16_t visibleCount = 0;
  uint16_t gsvVisibleTotal = 0;

  for (size_t i = 0; i < kMaxSatellites; ++i) {
    if (!satellites_[i].active) {
      continue;
    }

    if (now - satellites_[i].lastSeenMs > config::kSatelliteKeepaliveMs) {
      satellites_[i].active = false;
      satellites_[i].used = false;
      continue;
    }

    satellites_[i].used = isSatelliteUsed(satellites_[i].system, satellites_[i].id);
    ++visibleCount;
  }

  for (size_t i = 0; i < gsvStateCount_; ++i) {
    if (now - gsvStates_[i].lastUpdateMs <= config::kSatelliteKeepaliveMs) {
      gsvVisibleTotal =
          static_cast<uint16_t>(gsvVisibleTotal + gsvStates_[i].reportedVisible);
    }
  }

  status_.gsaUsedSatellites = static_cast<uint8_t>(usedSatelliteCount_);
  status_.gsvReportedVisible = gsvVisibleTotal;
  status_.visibleSatellites =
      gsvVisibleTotal > visibleCount ? gsvVisibleTotal : visibleCount;
}

size_t GnssManager::buildSortedSatelliteIndex(size_t indices[],
                                              size_t maxIndices) const {
  const uint32_t now = millis();
  size_t count = 0;

  for (size_t i = 0; i < kMaxSatellites && count < maxIndices; ++i) {
    if (satellites_[i].active &&
        now - satellites_[i].lastSeenMs <= config::kSatelliteKeepaliveMs) {
      indices[count++] = i;
    }
  }

  for (size_t i = 0; i < count; ++i) {
    size_t best = i;
    for (size_t j = i + 1; j < count; ++j) {
      const SatelliteInfo& a = satellites_[indices[best]];
      const SatelliteInfo& b = satellites_[indices[j]];

      const int aUsed = a.used ? 1 : 0;
      const int bUsed = b.used ? 1 : 0;
      const int aCno = a.cnoDbHz >= 0 ? a.cnoDbHz : -1;
      const int bCno = b.cnoDbHz >= 0 ? b.cnoDbHz : -1;

      if (bUsed > aUsed || (bUsed == aUsed && bCno > aCno) ||
          (bUsed == aUsed && bCno == aCno && strcmp(b.system, a.system) < 0) ||
          (bUsed == aUsed && bCno == aCno &&
           strcmp(b.system, a.system) == 0 && b.id < a.id)) {
        best = j;
      }
    }

    if (best != i) {
      const size_t tmp = indices[i];
      indices[i] = indices[best];
      indices[best] = tmp;
    }
  }

  return count;
}

bool GnssManager::isSatelliteUsed(const char* system, uint16_t id) const {
  for (size_t i = 0; i < usedSatelliteCount_; ++i) {
    if (satelliteRefEquals(usedSatellites_[i], system, id)) {
      return true;
    }
  }
  return false;
}

void GnssManager::clearUsedSatellites() {
  usedSatelliteCount_ = 0;
  for (size_t i = 0; i < kMaxSatellites; ++i) {
    satellites_[i].used = false;
  }
}

void GnssManager::addUsedSatellite(const char* system, uint16_t id) {
  if (id == 0) {
    return;
  }

  for (size_t i = 0; i < usedSatelliteCount_; ++i) {
    if (satelliteRefEquals(usedSatellites_[i], system, id)) {
      return;
    }
  }

  if (usedSatelliteCount_ >= kMaxUsedSatellites) {
    return;
  }

  strncpy(usedSatellites_[usedSatelliteCount_].system, system,
          sizeof(usedSatellites_[usedSatelliteCount_].system) - 1);
  usedSatellites_[usedSatelliteCount_]
      .system[sizeof(usedSatellites_[usedSatelliteCount_].system) - 1] = '\0';
  usedSatellites_[usedSatelliteCount_].id = id;
  ++usedSatelliteCount_;
}

SatelliteInfo* GnssManager::findSatellite(const char* system, uint16_t id) {
  for (size_t i = 0; i < kMaxSatellites; ++i) {
    if (satellites_[i].active && satellites_[i].id == id &&
        strcmp(satellites_[i].system, system) == 0) {
      return &satellites_[i];
    }
  }

  for (size_t i = 0; i < kMaxSatellites; ++i) {
    if (!satellites_[i].active) {
      satellites_[i] = SatelliteInfo{};
      satellites_[i].active = true;
      satellites_[i].id = id;
      strncpy(satellites_[i].system, system, sizeof(satellites_[i].system) - 1);
      satellites_[i].system[sizeof(satellites_[i].system) - 1] = '\0';
      return &satellites_[i];
    }
  }

  return nullptr;
}

GnssManager::GsvSystemState* GnssManager::getGsvSystemState(const char* system) {
  for (size_t i = 0; i < gsvStateCount_; ++i) {
    if (strcmp(gsvStates_[i].system, system) == 0) {
      return &gsvStates_[i];
    }
  }

  if (gsvStateCount_ >= kMaxGsvSystems) {
    return nullptr;
  }

  strncpy(gsvStates_[gsvStateCount_].system, system,
          sizeof(gsvStates_[gsvStateCount_].system) - 1);
  gsvStates_[gsvStateCount_].system[sizeof(gsvStates_[gsvStateCount_].system) - 1] =
      '\0';
  ++gsvStateCount_;
  return &gsvStates_[gsvStateCount_ - 1];
}
