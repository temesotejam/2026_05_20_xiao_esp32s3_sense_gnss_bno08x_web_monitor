#include "WebMonitor.h"

#include <WiFi.h>

#include "AppConfig.h"

namespace {

void appendJsonString(String& out, const char* key, const char* value,
                      bool comma = true) {
  out += "\"";
  out += key;
  out += "\":\"";
  out += value;
  out += "\"";
  if (comma) {
    out += ",";
  }
}

void appendJsonNumber(String& out, const char* key, double value, uint8_t digits,
                      bool comma = true) {
  out += "\"";
  out += key;
  out += "\":";
  out += String(value, static_cast<unsigned int>(digits));
  if (comma) {
    out += ",";
  }
}

void appendJsonUInt(String& out, const char* key, uint32_t value,
                    bool comma = true) {
  out += "\"";
  out += key;
  out += "\":";
  out += String(value);
  if (comma) {
    out += ",";
  }
}

void appendJsonBool(String& out, const char* key, bool value, bool comma = true) {
  out += "\"";
  out += key;
  out += "\":";
  out += value ? "true" : "false";
  if (comma) {
    out += ",";
  }
}

}  // namespace

WebMonitor::WebMonitor(GnssManager& gnss, ImuManager& imu,
                       SystemMonitor& system)
    : gnss_(gnss), imu_(imu), system_(system) {}

void WebMonitor::begin() {
  WiFi.mode(WIFI_AP);
  const bool apOk = WiFi.softAP(config::kApSsid, config::kApPassword);

  Serial.printf("[WiFi] AP %s: %s\n", apOk ? "started" : "failed",
                config::kApSsid);
  Serial.printf("[WiFi] IP: %s\n", WiFi.softAPIP().toString().c_str());

  server_.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "text/html; charset=utf-8", htmlPage());
  });

  server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response =
        request->beginResponse(200, "application/json", jsonStatus());
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  server_.onNotFound([](AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not found");
  });

  server_.begin();
  Serial.println("[Web] http://192.168.4.1/");
}

String WebMonitor::jsonStatus() const {
  const GnssStatus& g = gnss_.status();
  const ImuStatus& i = imu_.status();
  const SystemStatus& s = system_.status();

  String json;
  json.reserve(2600);
  json += "{";

  json += "\"gnss\":{";
  appendJsonString(json, "fix", gnss_.fixLabel());
  appendJsonBool(json, "nmeaReceived", g.nmeaReceived);
  appendJsonUInt(json, "satellites", g.visibleSatellites);
  appendJsonUInt(json, "usedSatellites",
                 g.gsaUsedSatellites > 0 ? g.gsaUsedSatellites
                                          : g.ggaUsedSatellites);
  appendJsonBool(json, "hasLocation", g.hasLocation);
  appendJsonNumber(json, "latitude", g.latitudeDeg, 7);
  appendJsonNumber(json, "longitude", g.longitudeDeg, 7);
  appendJsonBool(json, "hasSpeed", g.hasSpeed);
  appendJsonNumber(json, "speedKmh", g.speedKmh, 2);
  appendJsonBool(json, "hasCourse", g.hasCourse);
  appendJsonNumber(json, "courseDeg", g.courseDeg, 1);
  appendJsonBool(json, "hasHdop", g.hasHdop);
  appendJsonNumber(json, "hdop", g.hdop, 2);
  appendJsonUInt(json, "lastSentenceAgeMs",
                 g.nmeaReceived ? millis() - g.lastSentenceMs : 0, false);
  json += "},";

  json += "\"bno08x\":{";
  appendJsonBool(json, "initialized", i.initialized);
  appendJsonBool(json, "hasOrientation", i.hasOrientation);
  appendJsonUInt(json, "address", i.activeAddress);
  appendJsonNumber(json, "roll", i.rollDeg, 2);
  appendJsonNumber(json, "pitch", i.pitchDeg, 2);
  appendJsonNumber(json, "yaw", i.yawDeg, 2);
  appendJsonNumber(json, "updateHz", i.updateHz, 1);
  appendJsonUInt(json, "lastEventAgeMs",
                 i.lastEventMs > 0 ? millis() - i.lastEventMs : 0);
  appendJsonUInt(json, "resetCount", i.resetCount);
  appendJsonUInt(json, "timeoutCount", i.timeoutCount);
  appendJsonUInt(json, "initFailCount", i.initFailCount);
  appendJsonUInt(json, "reportFailCount", i.reportFailCount);
  appendJsonUInt(json, "lastResetAgeMs",
                 i.lastResetMs > 0 ? millis() - i.lastResetMs : 0);
  appendJsonUInt(json, "lastTimeoutAgeMs",
                 i.lastTimeoutMs > 0 ? millis() - i.lastTimeoutMs : 0);
  appendJsonUInt(json, "lastInitFailAgeMs",
                 i.lastInitFailMs > 0 ? millis() - i.lastInitFailMs : 0,
                 false);
  json += "},";

  json += "\"system\":{";
  appendJsonUInt(json, "millis", s.millisNow);
  appendJsonUInt(json, "freeHeap", s.freeHeap);
  appendJsonUInt(json, "wifiClients", s.wifiClients);
  appendJsonNumber(json, "loopHz", s.loopHz, 1, false);
  json += "},";

  json += "\"satellites\":[";
  const size_t count = gnss_.satelliteCount();
  const size_t limit = count > 24 ? 24 : count;
  for (size_t n = 0; n < limit; ++n) {
    const SatelliteInfo* sat = gnss_.satelliteAt(n);
    if (sat == nullptr) {
      continue;
    }
    if (n > 0) {
      json += ",";
    }
    json += "{";
    appendJsonString(json, "system", sat->system);
    appendJsonUInt(json, "id", sat->id);
    appendJsonBool(json, "used", sat->used);
    appendJsonUInt(json, "cno", sat->cnoDbHz >= 0 ? sat->cnoDbHz : 0);
    appendJsonUInt(json, "elevation",
                   sat->elevationDeg >= 0 ? sat->elevationDeg : 0);
    appendJsonUInt(json, "azimuth",
                   sat->azimuthDeg >= 0 ? sat->azimuthDeg : 0, false);
    json += "}";
  }
  json += "]";

  json += "}";
  return json;
}

String WebMonitor::htmlPage() const {
  String page;
  page.reserve(7600);
  page += F(R"rawliteral(
<!doctype html>
<html lang="ja">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>XIAO Boat Monitor</title>
<style>
:root{color-scheme:dark;--bg:#101418;--panel:#1b2229;--line:#303a44;--text:#eef3f7;--muted:#93a4b1;--ok:#55d68b;--warn:#ffd166;--bad:#ff6b6b;--accent:#62b6ff}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}
header{padding:18px 16px;border-bottom:1px solid var(--line);background:#151a1f;position:sticky;top:0}
h1{font-size:20px;margin:0 0 4px}.sub{color:var(--muted);font-size:13px}
main{padding:14px;display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(260px,1fr))}
section{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:14px}
h2{font-size:15px;margin:0 0 12px;color:#cfe5f8}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px 12px}.item{min-width:0}.label{color:var(--muted);font-size:12px}.value{font-size:20px;line-height:1.25;word-break:break-word}
.status{display:inline-block;padding:3px 8px;border-radius:999px;background:#26313b;font-size:13px}.ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}
table{width:100%;border-collapse:collapse;font-size:13px}th,td{border-bottom:1px solid var(--line);padding:6px;text-align:right}th:first-child,td:first-child{text-align:left}th{color:var(--muted);font-weight:600}
footer{padding:0 14px 14px;color:var(--muted);font-size:12px}
@media(max-width:560px){header{padding:14px}main{padding:10px;grid-template-columns:1fr}.value{font-size:18px}}
</style>
</head>
<body>
<header>
  <h1>XIAO Boat Monitor</h1>
  <div class="sub">AP: XIAO_BOAT_MONITOR / API: /api/status</div>
</header>
<main>
  <section>
    <h2>GNSS</h2>
    <div class="grid">
      <div class="item"><div class="label">Fix</div><div id="gnssFix" class="value">--</div></div>
      <div class="item"><div class="label">Satellites</div><div id="gnssSat" class="value">--</div></div>
      <div class="item"><div class="label">Latitude</div><div id="gnssLat" class="value">--</div></div>
      <div class="item"><div class="label">Longitude</div><div id="gnssLon" class="value">--</div></div>
      <div class="item"><div class="label">Speed</div><div id="gnssSpeed" class="value">--</div></div>
      <div class="item"><div class="label">Course</div><div id="gnssCourse" class="value">--</div></div>
      <div class="item"><div class="label">HDOP</div><div id="gnssHdop" class="value">--</div></div>
      <div class="item"><div class="label">NMEA age</div><div id="gnssAge" class="value">--</div></div>
    </div>
  </section>
  <section>
    <h2>BNO08X</h2>
    <div class="grid">
      <div class="item"><div class="label">Status</div><div id="imuStatus" class="value">--</div></div>
      <div class="item"><div class="label">UpdateHz</div><div id="imuHz" class="value">--</div></div>
      <div class="item"><div class="label">Roll</div><div id="imuRoll" class="value">--</div></div>
      <div class="item"><div class="label">Pitch</div><div id="imuPitch" class="value">--</div></div>
      <div class="item"><div class="label">Yaw</div><div id="imuYaw" class="value">--</div></div>
      <div class="item"><div class="label">Data age</div><div id="imuAge" class="value">--</div></div>
      <div class="item"><div class="label">Reset count</div><div id="imuReset" class="value">--</div></div>
      <div class="item"><div class="label">Timeout count</div><div id="imuTimeout" class="value">--</div></div>
      <div class="item"><div class="label">Init fail</div><div id="imuInitFail" class="value">--</div></div>
      <div class="item"><div class="label">Report fail</div><div id="imuReportFail" class="value">--</div></div>
      <div class="item"><div class="label">Last reset</div><div id="imuLastReset" class="value">--</div></div>
      <div class="item"><div class="label">Last timeout</div><div id="imuLastTimeout" class="value">--</div></div>
    </div>
  </section>
  <section>
    <h2>System</h2>
    <div class="grid">
      <div class="item"><div class="label">millis</div><div id="sysMillis" class="value">--</div></div>
      <div class="item"><div class="label">Free heap</div><div id="sysHeap" class="value">--</div></div>
      <div class="item"><div class="label">WiFi clients</div><div id="sysClients" class="value">--</div></div>
      <div class="item"><div class="label">LoopHz</div><div id="sysLoop" class="value">--</div></div>
    </div>
  </section>
  <section>
    <h2>Satellites</h2>
    <table><thead><tr><th>SYS</th><th>ID</th><th>USE</th><th>CNO</th><th>EL</th><th>AZ</th></tr></thead><tbody id="satRows"></tbody></table>
  </section>
</main>
<footer id="lastUpdate">Waiting for data...</footer>
<script>
const $=id=>document.getElementById(id);
const num=(v,d=1)=>Number.isFinite(v)?v.toFixed(d):"--";
const age=ms=>ms?`${ms} ms`:"--";
async function refresh(){
  try{
    const r=await fetch('/api/status',{cache:'no-store'});
    const d=await r.json();
    $('gnssFix').innerHTML=`<span class="status ${d.gnss.fix==='No Fix'?'warn':'ok'}">${d.gnss.fix}</span>`;
    $('gnssSat').textContent=d.gnss.satellites;
    $('gnssLat').textContent=d.gnss.hasLocation?num(d.gnss.latitude,7):'--';
    $('gnssLon').textContent=d.gnss.hasLocation?num(d.gnss.longitude,7):'--';
    $('gnssSpeed').textContent=d.gnss.hasSpeed?`${num(d.gnss.speedKmh,2)} km/h`:'--';
    $('gnssCourse').textContent=d.gnss.hasCourse?`${num(d.gnss.courseDeg,1)} deg`:'--';
    $('gnssHdop').textContent=d.gnss.hasHdop?num(d.gnss.hdop,2):'--';
    $('gnssAge').textContent=age(d.gnss.lastSentenceAgeMs);
    $('imuStatus').innerHTML=`<span class="status ${d.bno08x.initialized?'ok':'bad'}">${d.bno08x.initialized?'OK':'OFFLINE'}</span>`;
    $('imuHz').textContent=num(d.bno08x.updateHz,1);
    $('imuRoll').textContent=d.bno08x.hasOrientation?`${num(d.bno08x.roll,2)} deg`:'--';
    $('imuPitch').textContent=d.bno08x.hasOrientation?`${num(d.bno08x.pitch,2)} deg`:'--';
    $('imuYaw').textContent=d.bno08x.hasOrientation?`${num(d.bno08x.yaw,2)} deg`:'--';
    $('imuAge').textContent=age(d.bno08x.lastEventAgeMs);
    $('imuReset').textContent=d.bno08x.resetCount;
    $('imuTimeout').textContent=d.bno08x.timeoutCount;
    $('imuInitFail').textContent=d.bno08x.initFailCount;
    $('imuReportFail').textContent=d.bno08x.reportFailCount;
    $('imuLastReset').textContent=age(d.bno08x.lastResetAgeMs);
    $('imuLastTimeout').textContent=age(d.bno08x.lastTimeoutAgeMs);
    $('sysMillis').textContent=d.system.millis;
    $('sysHeap').textContent=d.system.freeHeap;
    $('sysClients').textContent=d.system.wifiClients;
    $('sysLoop').textContent=num(d.system.loopHz,1);
    $('satRows').innerHTML=d.satellites.map(s=>`<tr><td>${s.system}</td><td>${s.id}</td><td>${s.used?'*':'-'}</td><td>${s.cno||'--'}</td><td>${s.elevation||'--'}</td><td>${s.azimuth||'--'}</td></tr>`).join('')||'<tr><td colspan="6">--</td></tr>';
    $('lastUpdate').textContent=`Last update: ${new Date().toLocaleTimeString()}`;
  }catch(e){$('lastUpdate').textContent='Update failed';}
}
refresh();
setInterval(refresh, 1000);
</script>
</body>
</html>
)rawliteral");
  return page;
}
