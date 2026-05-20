#include "WebMonitor.h"

#include <WiFi.h>

#include "AppConfig.h"

namespace {

constexpr uint16_t kDnsPort = 53;

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
                       SystemMonitor& system, SdLogger& sdLogger)
    : gnss_(gnss), imu_(imu), system_(system), sdLogger_(sdLogger) {}

void WebMonitor::begin() {
  WiFi.mode(WIFI_AP);
  const bool apOk = WiFi.softAP(config::kApSsid, config::kApPassword);
  const IPAddress apIp = WiFi.softAPIP();

  Serial.printf("[WiFi] AP %s: %s\n", apOk ? "started" : "failed",
                config::kApSsid);
  Serial.printf("[WiFi] IP: %s\n", apIp.toString().c_str());

  if (config::kEnableCaptiveDnsWildcard) {
    dnsServer_.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer_.start(kDnsPort, "*", apIp);
    Serial.println("[DNS] captive portal wildcard DNS started");
  } else {
    Serial.println("[DNS] wildcard DNS disabled for online map mode");
  }

  server_.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "text/html; charset=utf-8", htmlPage());
  });

  server_.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->redirect("/");
  });

  server_.on("/hotspot-detect.html", HTTP_GET,
             [](AsyncWebServerRequest* request) {
               request->send(200, "text/html; charset=utf-8",
                             "<!doctype html><html><head><title>XIAO Boat "
                             "Monitor</title></head><body><a "
                             "href=\"/\">XIAO Boat Monitor</a></body></html>");
             });

  server_.on("/connecttest.txt", HTTP_GET,
             [](AsyncWebServerRequest* request) {
               request->send(200, "text/plain", "XIAO Boat Monitor");
             });

  server_.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "XIAO Boat Monitor");
  });

  server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response =
        request->beginResponse(200, "application/json", jsonStatus());
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  server_.onNotFound([](AsyncWebServerRequest* request) {
    request->redirect("/");
  });

  server_.begin();
  Serial.println("[Web] http://192.168.4.1/");
}

void WebMonitor::update() {
  if (config::kEnableCaptiveDnsWildcard) {
    dnsServer_.processNextRequest();
  }
}

String WebMonitor::jsonStatus() const {
  const GnssStatus& g = gnss_.status();
  const ImuStatus& i = imu_.status();
  const SystemStatus& s = system_.status();
  const SdLogStatus& sd = sdLogger_.status();

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
  appendJsonUInt(json, "accuracy", i.accuracy);
  appendJsonString(json, "reportType", i.reportType);
  appendJsonUInt(json, "consecutiveFailCount", i.consecutiveFailCount);
  appendJsonUInt(json, "nextRetryDelayMs", i.nextRetryDelayMs);
  appendJsonUInt(json, "recoveryCount", i.recoveryCount);
  appendJsonUInt(json, "lastResetAgeMs",
                 i.lastResetMs > 0 ? millis() - i.lastResetMs : 0);
  appendJsonUInt(json, "lastTimeoutAgeMs",
                 i.lastTimeoutMs > 0 ? millis() - i.lastTimeoutMs : 0);
  appendJsonUInt(json, "lastRecoveryAgeMs",
                 i.lastRecoveryMs > 0 ? millis() - i.lastRecoveryMs : 0);
  appendJsonUInt(json, "lastInitFailAgeMs",
                 i.lastInitFailMs > 0 ? millis() - i.lastInitFailMs : 0,
                 false);
  json += "},";

  json += "\"sd\":{";
  appendJsonBool(json, "mounted", sd.mounted);
  appendJsonBool(json, "fileReady", sd.fileReady);
  appendJsonUInt(json, "csPin", sd.csPin >= 0 ? sd.csPin : 0);
  appendJsonString(json, "filePath", sd.filePath);
  appendJsonString(json, "lastStatus", sd.lastStatus);
  appendJsonUInt(json, "writeCount", sd.writeCount);
  appendJsonUInt(json, "writeErrorCount", sd.writeErrorCount);
  appendJsonUInt(json, "lastWriteAgeMs",
                 sd.lastWriteMs > 0 ? millis() - sd.lastWriteMs : 0);
  appendJsonUInt(json, "lastErrorAgeMs",
                 sd.lastErrorMs > 0 ? millis() - sd.lastErrorMs : 0, false);
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
.wide{grid-column:1/-1}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px 12px}.item{min-width:0}.label{color:var(--muted);font-size:12px}.value{font-size:20px;line-height:1.25;word-break:break-word}
.status{display:inline-block;padding:3px 8px;border-radius:999px;background:#26313b;font-size:13px}.ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}
#map{width:100%;height:380px;min-height:320px;border:1px solid var(--line);border-radius:8px;background:#0d1115;display:block}.mapbar{display:flex;gap:10px;align-items:center;justify-content:space-between;margin-bottom:10px;color:var(--muted);font-size:13px;flex-wrap:wrap}.seg{display:inline-flex;border:1px solid var(--line);border-radius:8px;overflow:hidden}.seg button{background:#111820;color:var(--muted);border:0;border-right:1px solid var(--line);padding:8px 10px;font:inherit}.seg button:last-child{border-right:0}.seg button.active{background:#26313b;color:var(--text)}
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
  <section class="wide">
    <h2>Map</h2>
    <div class="mapbar">
      <div id="mapStatus">Waiting for GNSS fix</div>
      <div class="seg"><button id="mapNorth" class="active" type="button">North Up</button><button id="mapHeading" type="button">Heading Up</button></div>
    </div>
    <canvas id="map"></canvas>
  </section>
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
      <div class="item"><div class="label">Accuracy</div><div id="imuAccuracy" class="value">--</div></div>
      <div class="item"><div class="label">Report</div><div id="imuReportType" class="value">--</div></div>
      <div class="item"><div class="label">Consecutive fail</div><div id="imuConsecutiveFail" class="value">--</div></div>
      <div class="item"><div class="label">Next retry</div><div id="imuNextRetry" class="value">--</div></div>
      <div class="item"><div class="label">Recoveries</div><div id="imuRecovery" class="value">--</div></div>
      <div class="item"><div class="label">Last reset</div><div id="imuLastReset" class="value">--</div></div>
      <div class="item"><div class="label">Last timeout</div><div id="imuLastTimeout" class="value">--</div></div>
      <div class="item"><div class="label">Last recovery</div><div id="imuLastRecovery" class="value">--</div></div>
    </div>
  </section>
  <section>
    <h2>SD Log</h2>
    <div class="grid">
      <div class="item"><div class="label">Mounted</div><div id="sdMounted" class="value">--</div></div>
      <div class="item"><div class="label">File ready</div><div id="sdReady" class="value">--</div></div>
      <div class="item"><div class="label">File</div><div id="sdFile" class="value">--</div></div>
      <div class="item"><div class="label">Status</div><div id="sdStatus" class="value">--</div></div>
      <div class="item"><div class="label">Rows</div><div id="sdRows" class="value">--</div></div>
      <div class="item"><div class="label">Errors</div><div id="sdErrors" class="value">--</div></div>
      <div class="item"><div class="label">Last write</div><div id="sdLastWrite" class="value">--</div></div>
      <div class="item"><div class="label">Last error</div><div id="sdLastError" class="value">--</div></div>
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
let mapMode='north';
let origin=null;
let latestSnapshot=null;
const track=[];
function normalizeDeg(v){return ((v%360)+360)%360;}
function headingFrom(d){
  if(d.bno08x.hasOrientation)return {deg:normalizeDeg(d.bno08x.yaw),src:'IMU yaw'};
  if(d.gnss.hasCourse)return {deg:normalizeDeg(d.gnss.courseDeg),src:'GNSS course'};
  return {deg:0,src:'none'};
}
function metersFromOrigin(lat,lon){
  const latRad=origin.lat*Math.PI/180;
  return {
    x:(lon-origin.lon)*111320*Math.cos(latRad),
    y:(lat-origin.lat)*110540
  };
}
function rotatePoint(p,deg){
  if(mapMode==='north')return p;
  const r=deg*Math.PI/180;
  return {x:p.x*Math.cos(r)-p.y*Math.sin(r),y:p.x*Math.sin(r)+p.y*Math.cos(r)};
}
function setMapMode(mode){
  mapMode=mode;
  $('mapNorth').classList.toggle('active',mode==='north');
  $('mapHeading').classList.toggle('active',mode==='heading');
  if(latestSnapshot)drawMap(latestSnapshot);
}
$('mapNorth').onclick=()=>setMapMode('north');
$('mapHeading').onclick=()=>setMapMode('heading');
function drawGrid(ctx,w,h,scale,range){
  const cx=w/2,cy=h/2;
  ctx.strokeStyle='#26313b';
  ctx.lineWidth=1;
  let step=5;
  if(range>40)step=10;
  if(range>100)step=25;
  if(range>250)step=50;
  if(range>600)step=100;
  const maxMeters=Math.ceil(range/step)*step;
  for(let m=-maxMeters;m<=maxMeters;m+=step){
    const p=m*scale;
    ctx.beginPath();ctx.moveTo(cx+p,0);ctx.lineTo(cx+p,h);ctx.stroke();
    ctx.beginPath();ctx.moveTo(0,cy+p);ctx.lineTo(w,cy+p);ctx.stroke();
  }
  ctx.fillStyle='#93a4b1';
  ctx.font='12px system-ui';
  ctx.fillText(`${step} m grid`,12,h-12);
}
function drawBoat(ctx,cx,cy,headingDeg){
  const r=headingDeg*Math.PI/180;
  const pts=[[0,-18],[11,14],[0,8],[-11,14]];
  ctx.beginPath();
  pts.forEach((p,i)=>{
    const x=p[0]*Math.cos(r)-p[1]*Math.sin(r);
    const y=p[0]*Math.sin(r)+p[1]*Math.cos(r);
    if(i===0)ctx.moveTo(cx+x,cy+y);else ctx.lineTo(cx+x,cy+y);
  });
  ctx.closePath();
  ctx.fillStyle='#62b6ff';
  ctx.strokeStyle='#eef3f7';
  ctx.lineWidth=2;
  ctx.fill();ctx.stroke();
}
function drawNorthArrow(ctx,w){
  ctx.save();
  ctx.translate(w-28,30);
  ctx.fillStyle='#eef3f7';
  ctx.font='12px system-ui';
  ctx.textAlign='center';
  ctx.fillText('N',0,-14);
  ctx.beginPath();
  ctx.moveTo(0,-8);ctx.lineTo(7,10);ctx.lineTo(0,6);ctx.lineTo(-7,10);
  ctx.closePath();ctx.fill();
  ctx.restore();
}
function drawMap(d){
  latestSnapshot=d;
  const c=$('map'),ctx=c.getContext('2d');
  const rect=c.getBoundingClientRect();
  const ratio=window.devicePixelRatio||1;
  const w=Math.max(280,Math.floor(rect.width));
  const h=Math.max(320,Math.floor(rect.height));
  if(c.width!==Math.floor(w*ratio)||c.height!==Math.floor(h*ratio)){
    c.width=Math.floor(w*ratio);c.height=Math.floor(h*ratio);
  }
  ctx.setTransform(ratio,0,0,ratio,0,0);
  ctx.clearRect(0,0,w,h);
  ctx.fillStyle='#0d1115';
  ctx.fillRect(0,0,w,h);
  if(!d.gnss.hasLocation){
    $('mapStatus').textContent='Waiting for GNSS fix';
    ctx.fillStyle='#93a4b1';
    ctx.font='16px system-ui';
    ctx.textAlign='center';
    ctx.fillText('Waiting for GNSS fix',w/2,h/2);
    return;
  }
  if(!origin)origin={lat:d.gnss.latitude,lon:d.gnss.longitude};
  const now=metersFromOrigin(d.gnss.latitude,d.gnss.longitude);
  track.push(now);
  if(track.length>600)track.shift();
  const heading=headingFrom(d);
  const centered=track.map(p=>({x:p.x-now.x,y:p.y-now.y}));
  const rotated=centered.map(p=>rotatePoint(p,heading.deg));
  let range=20;
  rotated.forEach(p=>{range=Math.max(range,Math.abs(p.x),Math.abs(p.y));});
  const scale=Math.min(w,h)*0.43/range;
  const cx=w/2,cy=h/2;
  drawGrid(ctx,w,h,scale,range);
  drawNorthArrow(ctx,w);
  ctx.strokeStyle='#62b6ff';
  ctx.lineWidth=3;
  ctx.beginPath();
  rotated.forEach((p,i)=>{
    const x=cx+p.x*scale;
    const y=cy-p.y*scale;
    if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
  });
  ctx.stroke();
  const boatScreenHeading=mapMode==='north'?heading.deg:0;
  drawBoat(ctx,cx,cy,boatScreenHeading);
  ctx.fillStyle='#eef3f7';
  ctx.font='13px system-ui';
  ctx.textAlign='left';
  ctx.fillText(`Mode: ${mapMode==='north'?'North Up':'Heading Up'}`,12,20);
  ctx.fillText(`Heading: ${num(heading.deg,1)} deg (${heading.src})`,12,40);
  ctx.fillText(`Origin: ${num(origin.lat,6)}, ${num(origin.lon,6)}`,12,60);
  $('mapStatus').textContent=`${num(d.gnss.latitude,7)}, ${num(d.gnss.longitude,7)} / ${track.length} pts / ${mapMode==='north'?'north up':'heading up'}`;
}
function updateMap(d){
  if(d.gnss.hasLocation){
    drawMap(d);
  }else{
    latestSnapshot=d;
    drawMap(d);
  }
}
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
    updateMap(d);
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
    $('imuAccuracy').textContent=d.bno08x.accuracy;
    $('imuReportType').textContent=d.bno08x.reportType;
    $('imuConsecutiveFail').textContent=d.bno08x.consecutiveFailCount;
    $('imuNextRetry').textContent=age(d.bno08x.nextRetryDelayMs);
    $('imuRecovery').textContent=d.bno08x.recoveryCount;
    $('imuLastReset').textContent=age(d.bno08x.lastResetAgeMs);
    $('imuLastTimeout').textContent=age(d.bno08x.lastTimeoutAgeMs);
    $('imuLastRecovery').textContent=age(d.bno08x.lastRecoveryAgeMs);
    $('sdMounted').innerHTML=`<span class="status ${d.sd.mounted?'ok':'bad'}">${d.sd.mounted?'YES':'NO'}</span>`;
    $('sdReady').innerHTML=`<span class="status ${d.sd.fileReady?'ok':'bad'}">${d.sd.fileReady?'YES':'NO'}</span>`;
    $('sdFile').textContent=d.sd.filePath;
    $('sdStatus').textContent=d.sd.lastStatus;
    $('sdRows').textContent=d.sd.writeCount;
    $('sdErrors').textContent=d.sd.writeErrorCount;
    $('sdLastWrite').textContent=age(d.sd.lastWriteAgeMs);
    $('sdLastError').textContent=age(d.sd.lastErrorAgeMs);
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
