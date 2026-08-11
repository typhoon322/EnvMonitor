#include "net/web_ui.h"

#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "net/deepseek_monitor.h"
#include "net/mqtt_telemetry.h"
#include "net/wifi_manager.h"
#include "storage/settings_store.h"

namespace {
WebServer server(80);

const char kIndexHtml[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>EnvMonitor</title>
<style>
body{font-family:system-ui,sans-serif;margin:0;background:#0f1419;color:#e7ecf1}
header{padding:14px 16px;background:#1a2332;border-bottom:1px solid #2a3544}
h1{margin:0;font-size:1.25rem;color:#5eead4}
h2{font-size:1rem;margin:18px 0 8px;color:#94a3b8}
main{padding:12px 16px 32px;max-width:640px}
.card{background:#1a2332;border-radius:10px;padding:12px;margin-bottom:12px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.kv{font-size:.9rem}.kv b{color:#5eead4;display:block;font-size:.75rem;font-weight:600}
label{display:block;font-size:.75rem;color:#94a3b8;margin-top:8px}
input,select,button{width:100%;box-sizing:border-box;margin-top:4px;padding:8px;border-radius:6px;border:1px solid #2a3544;background:#0f1419;color:#e7ecf1}
button{background:#0d9488;border:none;font-weight:600;cursor:pointer;margin-top:10px}
button.secondary{background:#334155}
.msg{font-size:.8rem;color:#fbbf24;min-height:1.2em;margin-top:6px}
.ok{color:#4ade80}.err{color:#f87171}
</style>
</head>
<body>
<header><h1>EnvMonitor</h1><div id="net" class="msg"></div></header>
<main>
<section class="card">
<h2>环境数据</h2>
<div class="grid">
<div class="kv"><b>温度</b><span id="t">--</span></div>
<div class="kv"><b>湿度</b><span id="h">--</span></div>
<div class="kv"><b>eCO2</b><span id="e">--</span></div>
<div class="kv"><b>TVOC</b><span id="v">--</span></div>
<div class="kv"><b>AQI</b><span id="a">--</span></div>
<div class="kv"><b>传感器</b><span id="s">--</span></div>
</div>
</section>
<section class="card">
<h2>DeepSeek</h2>
<div id="ds" class="msg">--</div>
</section>
<section class="card">
<h2>WiFi</h2>
<label>SSID</label><input id="wifiSsid"/>
<label>密码</label><input id="wifiPass" type="password"/>
<button onclick="saveWifi()">保存并连接</button>
<div id="wifiMsg" class="msg"></div>
</section>
<section class="card">
<h2>MQTT</h2>
<label>Host</label><input id="mqttHost"/>
<label>Port</label><input id="mqttPort" type="number"/>
<label>User</label><input id="mqttUser"/>
<label>Pass</label><input id="mqttPass" type="password"/>
<label>Prefix</label><input id="mqttPrefix"/>
<label>Device ID</label><input id="mqttId"/>
<label>Interval (s)</label><input id="mqttIntv" type="number"/>
<button onclick="saveMqtt()">保存 MQTT</button>
<div id="mqttMsg" class="msg"></div>
</section>
<section class="card">
<h2>DeepSeek Keys</h2>
<label>名称</label><input id="dsName" placeholder="main"/>
<label>API Key</label><input id="dsKey"/>
<button onclick="dsAdd()">添加 / 更新</button>
<label>删除（名称或序号）</label><input id="dsDel"/>
<button class="secondary" onclick="dsRemove()">删除</button>
<label>刷新间隔 (s)</label><input id="dsIntv" type="number"/>
<button class="secondary" onclick="dsInterval()">设置间隔</button>
<button class="secondary" onclick="dsRefresh()">立即刷新</button>
<div id="dsMsg" class="msg"></div>
</section>
<section class="card">
<h2>TFT 视图</h2>
<select id="view">
<option value="status">status</option>
<option value="chart">chart</option>
<option value="ds">deepseek</option>
</select>
<button onclick="saveView()">切换视图</button>
<div id="viewMsg" class="msg"></div>
</section>
</main>
<script>
async function j(u,opt){const r=await fetch(u,opt);return r.json()}
function setMsg(id,t,ok){const e=document.getElementById(id);e.textContent=t;e.className='msg '+(ok?'ok':'err')}
async function refresh(){
  try{
    const d=await j('/api/status');
    document.getElementById('t').textContent=d.temp!=null?d.temp.toFixed(1)+' C':'N/A';
    document.getElementById('h').textContent=d.hum!=null?d.hum.toFixed(1)+' %':'N/A';
    document.getElementById('e').textContent=d.eco2+' ppm';
    document.getElementById('v').textContent=d.tvoc+' ppb';
    document.getElementById('a').textContent=d.aqi+'/5';
    document.getElementById('s').textContent=d.sensor||'?';
    let net='STA: '+(d.wifiConnected?(d.staIp+' ('+d.ssid+')'):'未连接');
    if(d.apActive) net+=' | AP: '+d.apSsid+' '+d.apIp;
    document.getElementById('net').textContent=net;
    if(!d.dsKeys||d.dsKeys.length===0){
      document.getElementById('ds').textContent='未配置 API Key — 请在下方添加';
    }else{
      document.getElementById('ds').textContent=d.dsKeys.map(k=>k.name+': '+(k.valid?(k.currency+' '+k.total):(k.error||'pending'))).join(' | ');
    }
    document.getElementById('view').value=d.view||'status';
  }catch(e){}
}
async function loadCfg(){
  const c=await j('/api/config');
  document.getElementById('wifiSsid').value=c.wifiSsid||'';
  document.getElementById('mqttHost').value=c.mqttHost||'';
  document.getElementById('mqttPort').value=c.mqttPort||1883;
  document.getElementById('mqttUser').value=c.mqttUser||'';
  document.getElementById('mqttPrefix').value=c.mqttPrefix||'';
  document.getElementById('mqttId').value=c.deviceId||'';
  document.getElementById('mqttIntv').value=c.mqttInterval||10;
  document.getElementById('dsIntv').value=c.dsInterval||180;
}
async function saveWifi(){
  const body={ssid:wifiSsid.value,pass:wifiPass.value};
  const r=await j('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  setMsg('wifiMsg',r.ok?'已保存，正在连接':'失败',r.ok);
}
async function saveMqtt(){
  const body={host:mqttHost.value,port:+mqttPort.value,user:mqttUser.value,pass:mqttPass.value,prefix:mqttPrefix.value,deviceId:mqttId.value,interval:+mqttIntv.value};
  const r=await j('/api/mqtt',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  setMsg('mqttMsg',r.ok?'MQTT 已保存':'失败',r.ok);
}
async function dsAdd(){
  const r=await j('/api/deepseek',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'add',name:dsName.value,apiKey:dsKey.value})});
  setMsg('dsMsg',r.ok?'Key 已保存':'失败',r.ok); if(r.ok) refresh();
}
async function dsRemove(){
  const r=await j('/api/deepseek',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'del',name:dsDel.value})});
  setMsg('dsMsg',r.ok?'已删除':'失败',r.ok); if(r.ok) refresh();
}
async function dsInterval(){
  const r=await j('/api/deepseek',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'interval',interval:+dsIntv.value})});
  setMsg('dsMsg',r.ok?'间隔已更新':'失败',r.ok);
}
async function dsRefresh(){
  const r=await j('/api/deepseek',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'refresh'})});
  setMsg('dsMsg',r.ok?'刷新已排队':'失败',r.ok);
}
async function saveView(){
  const r=await j('/api/view',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({view:view.value})});
  setMsg('viewMsg',r.ok?'视图已切换':'失败',r.ok);
}
loadCfg(); refresh(); setInterval(refresh,2000);
</script>
</body>
</html>
)HTML";
}  // namespace

void WebUi::sendJson_(int code, const String &body) {
  server.send(code, F("application/json"), body);
}

void WebUi::handleRoot_() {
  server.send_P(200, "text/html", kIndexHtml);
}

void WebUi::handleStatus_() {
  JsonDocument doc;
  if (ctx_.reading != nullptr) {
    if (!isnan(ctx_.reading->temperatureC)) {
      doc["temp"] = ctx_.reading->temperatureC;
    }
    if (!isnan(ctx_.reading->humidityPct)) {
      doc["hum"] = ctx_.reading->humidityPct;
    }
    doc["eco2"] = ctx_.reading->eco2Ppm;
    doc["tvoc"] = ctx_.reading->tvocPpb;
    doc["aqi"] = ctx_.reading->aqiUba;
  }
  if (ctx_.sensor != nullptr) {
    doc["sensor"] = ctx_.sensor->stateText();
  }
  if (ctx_.wifi != nullptr) {
    doc["wifiConnected"] = ctx_.wifi->isConnected();
    doc["ssid"] = ctx_.wifi->staSsid();
    doc["staIp"] = ctx_.wifi->localIP().toString();
    doc["apActive"] = ctx_.wifi->isApActive();
    doc["apSsid"] = ctx_.wifi->apSsid();
    doc["apIp"] = ctx_.wifi->apIP().toString();
  }
  if (ctx_.displayView != nullptr) {
    switch (*ctx_.displayView) {
      case DisplayView::Chart:
        doc["view"] = "chart";
        break;
      case DisplayView::DeepSeek:
        doc["view"] = "ds";
        break;
      default:
        doc["view"] = "status";
        break;
    }
  }
  JsonArray keys = doc["dsKeys"].to<JsonArray>();
  if (ctx_.deepseek != nullptr) {
    doc["dsInterval"] = ctx_.deepseek->intervalSec();
    doc["dsRefreshing"] = ctx_.deepseek->isRefreshing();
    for (uint8_t i = 0; i < ctx_.deepseek->keyCount(); ++i) {
      const DeepSeekBalanceEntry *e = ctx_.deepseek->balanceAt(i);
      if (e == nullptr) {
        continue;
      }
      JsonObject o = keys.add<JsonObject>();
      o["name"] = e->name;
      o["valid"] = e->valid;
      o["currency"] = e->currency;
      o["total"] = e->totalBalance;
      o["error"] = e->error;
    }
  }
  String out;
  serializeJson(doc, out);
  sendJson_(200, out);
}

void WebUi::handleConfig_() {
  JsonDocument doc;
  if (ctx_.settings != nullptr) {
    char ssid[33];
    char pass[65];
    ctx_.settings->loadWifiCredentials(ssid, sizeof(ssid), pass, sizeof(pass));
    doc["wifiSsid"] = ssid;
    doc["wifiPassSet"] = pass[0] != '\0';

    MqttConfig mqtt;
    ctx_.settings->loadMqttConfig(mqtt);
    doc["mqttHost"] = mqtt.host;
    doc["mqttPort"] = mqtt.port;
    doc["mqttUser"] = mqtt.user;
    doc["mqttPassSet"] = mqtt.pass[0] != '\0';
    doc["mqttPrefix"] = mqtt.prefix;
    doc["deviceId"] = mqtt.deviceId;
    doc["mqttInterval"] = mqtt.intervalSec;

    DeepSeekConfig ds;
    ctx_.settings->loadDeepSeekConfig(ds);
    doc["dsInterval"] = ds.intervalSec;
    doc["dsKeyCount"] = ds.keyCount;
  }
  String out;
  serializeJson(doc, out);
  sendJson_(200, out);
}

void WebUi::handleWifi_() {
  if (ctx_.wifi == nullptr) {
    sendJson_(500, F("{\"ok\":false}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    sendJson_(400, F("{\"ok\":false}"));
    return;
  }
  const char *ssid = doc["ssid"] | "";
  const char *pass = doc["pass"] | "";
  const bool ok = ctx_.wifi->setCredentials(ssid, pass);
  sendJson_(ok ? 200 : 400, ok ? F("{\"ok\":true}") : F("{\"ok\":false}"));
}

void WebUi::handleMqtt_() {
  if (ctx_.mqtt == nullptr || ctx_.settings == nullptr) {
    sendJson_(500, F("{\"ok\":false}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    sendJson_(400, F("{\"ok\":false}"));
    return;
  }
  MqttConfig cfg;
  ctx_.settings->loadMqttConfig(cfg);
  if (doc["host"].is<const char *>()) {
    strncpy(cfg.host, doc["host"] | "", sizeof(cfg.host) - 1);
  }
  if (doc["port"].is<int>()) {
    cfg.port = static_cast<uint16_t>(doc["port"].as<int>());
  }
  if (doc["user"].is<const char *>()) {
    strncpy(cfg.user, doc["user"] | "", sizeof(cfg.user) - 1);
  }
  if (doc["pass"].is<const char *>()) {
    const char *pass = doc["pass"] | "";
    if (pass[0] != '\0') {
      strncpy(cfg.pass, pass, sizeof(cfg.pass) - 1);
    }
  }
  if (doc["prefix"].is<const char *>()) {
    strncpy(cfg.prefix, doc["prefix"] | "", sizeof(cfg.prefix) - 1);
  }
  if (doc["deviceId"].is<const char *>()) {
    strncpy(cfg.deviceId, doc["deviceId"] | "", sizeof(cfg.deviceId) - 1);
  }
  if (doc["interval"].is<int>()) {
    cfg.intervalSec = static_cast<uint16_t>(doc["interval"].as<int>());
  }
  const bool ok = ctx_.mqtt->applyConfig(cfg);
  sendJson_(ok ? 200 : 400, ok ? F("{\"ok\":true}") : F("{\"ok\":false}"));
}

void WebUi::handleDeepSeek_() {
  if (ctx_.deepseek == nullptr) {
    sendJson_(500, F("{\"ok\":false}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    sendJson_(400, F("{\"ok\":false}"));
    return;
  }
  const char *action = doc["action"] | "";
  bool ok = false;
  if (strcmp(action, "add") == 0) {
    ok = ctx_.deepseek->addKey(doc["name"] | "", doc["apiKey"] | "");
  } else if (strcmp(action, "del") == 0) {
    ok = ctx_.deepseek->removeKey(doc["name"] | "");
  } else if (strcmp(action, "interval") == 0) {
    ok = ctx_.deepseek->setIntervalSec(static_cast<uint16_t>(doc["interval"] | 180));
  } else if (strcmp(action, "refresh") == 0) {
    ctx_.deepseek->requestRefresh();
    ok = true;
  }
  sendJson_(ok ? 200 : 400, ok ? F("{\"ok\":true}") : F("{\"ok\":false}"));
}

void WebUi::handleView_() {
  if (ctx_.displayView == nullptr || ctx_.settings == nullptr) {
    sendJson_(500, F("{\"ok\":false}"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    sendJson_(400, F("{\"ok\":false}"));
    return;
  }
  const char *view = doc["view"] | "";
  if (strcmp(view, "chart") == 0) {
    *ctx_.displayView = DisplayView::Chart;
  } else if (strcmp(view, "ds") == 0 || strcmp(view, "deepseek") == 0) {
    *ctx_.displayView = DisplayView::DeepSeek;
  } else if (strcmp(view, "status") == 0) {
    *ctx_.displayView = DisplayView::Status;
  } else {
    sendJson_(400, F("{\"ok\":false}"));
    return;
  }
  SystemContext sys{};
  sys.displayView = ctx_.displayView;
  ctx_.settings->save(sys);
  sendJson_(200, F("{\"ok\":true}"));
}

void WebUi::begin(const WebUiContext &ctx) {
  ctx_ = ctx;
  server.on("/", HTTP_GET, [this]() { handleRoot_(); });
  server.on("/api/status", HTTP_GET, [this]() { handleStatus_(); });
  server.on("/api/config", HTTP_GET, [this]() { handleConfig_(); });
  server.on("/api/wifi", HTTP_POST, [this]() { handleWifi_(); });
  server.on("/api/mqtt", HTTP_POST, [this]() { handleMqtt_(); });
  server.on("/api/deepseek", HTTP_POST, [this]() { handleDeepSeek_(); });
  server.on("/api/view", HTTP_POST, [this]() { handleView_(); });
  server.begin();
  started_ = true;
  Serial.println(F("WebUI listening on :80"));
}

void WebUi::tick() {
  if (started_) {
    server.handleClient();
  }
}
