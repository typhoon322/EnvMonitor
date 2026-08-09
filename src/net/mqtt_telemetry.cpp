#include "net/mqtt_telemetry.h"

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "config.h"
#include "net/wifi_manager.h"

namespace {
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void copyString(char *dest, size_t destLen, const char *src) {
  if (dest == nullptr || destLen == 0) {
    return;
  }
  strncpy(dest, src != nullptr ? src : "", destLen - 1);
  dest[destLen - 1] = '\0';
}
}  // namespace

void MqttTelemetry::resetReconnectBackoff_() {
  reconnectBackoffMs_ = MQTT_RECONNECT_MIN_MS;
  nextReconnectMs_ = 0;
}

void MqttTelemetry::scheduleReconnect_(uint32_t nowMs, bool failed) {
  if (!failed) {
    return;
  }
  nextReconnectMs_ = nowMs + reconnectBackoffMs_;
  Serial.print(F("MQTT connect failed; retry in "));
  Serial.print(reconnectBackoffMs_ / 1000UL);
  Serial.println(F("s"));
  if (reconnectBackoffMs_ < MQTT_RECONNECT_MAX_MS) {
    const uint32_t next = reconnectBackoffMs_ * 2;
    reconnectBackoffMs_ = next > MQTT_RECONNECT_MAX_MS ? MQTT_RECONNECT_MAX_MS : next;
  }
}

void MqttTelemetry::begin(SettingsStore *settings, WifiManager *wifi) {
  settings_ = settings;
  wifi_ = wifi;
  reloadConfig();
  mqttClient.setBufferSize(256);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(3);
}

void MqttTelemetry::reloadConfig() {
  if (settings_ != nullptr) {
    settings_->loadMqttConfig(cfg_);
  } else {
    cfg_ = MqttConfig{};
  }
  resolveDeviceId_();
  buildTopics_();
  lastPublishMs_ = 0;
  resetReconnectBackoff_();
  if (mqttClient.connected()) {
    mqttClient.disconnect();
  }
}

void MqttTelemetry::resolveDeviceId_() {
  if (cfg_.deviceId[0] != '\0') {
    copyString(deviceIdResolved_, sizeof(deviceIdResolved_), cfg_.deviceId);
    return;
  }
  const uint64_t mac = ESP.getEfuseMac();
  snprintf(deviceIdResolved_, sizeof(deviceIdResolved_), "%02X%02X%02X",
           static_cast<unsigned>((mac >> 16) & 0xFF), static_cast<unsigned>((mac >> 8) & 0xFF),
           static_cast<unsigned>(mac & 0xFF));
}

void MqttTelemetry::buildTopics_() {
  const char *prefix = cfg_.prefix[0] != '\0' ? cfg_.prefix : MQTT_DEFAULT_PREFIX;
  snprintf(topicState_, sizeof(topicState_), "%s/%s/state", prefix, deviceIdResolved_);
  snprintf(topicStatus_, sizeof(topicStatus_), "%s/%s/status", prefix, deviceIdResolved_);
}

bool MqttTelemetry::applyConfig(const MqttConfig &cfg) {
  cfg_ = cfg;
  if (cfg_.intervalSec < MQTT_INTERVAL_MIN_SEC) {
    cfg_.intervalSec = MQTT_INTERVAL_MIN_SEC;
  }
  if (cfg_.intervalSec > MQTT_INTERVAL_MAX_SEC) {
    cfg_.intervalSec = MQTT_INTERVAL_MAX_SEC;
  }
  if (cfg_.prefix[0] == '\0') {
    copyString(cfg_.prefix, sizeof(cfg_.prefix), MQTT_DEFAULT_PREFIX);
  }
  if (cfg_.port == 0) {
    cfg_.port = MQTT_DEFAULT_PORT;
  }
  if (settings_ != nullptr) {
    settings_->saveMqttConfig(cfg_);
  }
  resolveDeviceId_();
  buildTopics_();
  lastPublishMs_ = 0;
  resetReconnectBackoff_();
  if (mqttClient.connected()) {
    mqttClient.disconnect();
  }
  return true;
}

bool MqttTelemetry::setIntervalSec(uint16_t sec) {
  if (sec < MQTT_INTERVAL_MIN_SEC || sec > MQTT_INTERVAL_MAX_SEC) {
    return false;
  }
  cfg_.intervalSec = sec;
  if (settings_ != nullptr) {
    settings_->saveMqttConfig(cfg_);
  }
  return true;
}

bool MqttTelemetry::ensureConnected_() {
  if (wifi_ == nullptr || !wifi_->isConnected()) {
    return false;
  }
  if (cfg_.host[0] == '\0') {
    return false;
  }
  if (mqttClient.connected()) {
    return true;
  }

  mqttClient.setServer(cfg_.host, cfg_.port);
  const String clientId = String("envmon-") + deviceIdResolved_;
  bool ok = false;
  if (cfg_.user[0] != '\0') {
    ok = mqttClient.connect(clientId.c_str(), cfg_.user, cfg_.pass, topicStatus_, 1, true,
                            "offline");
  } else {
    ok = mqttClient.connect(clientId.c_str(), nullptr, nullptr, topicStatus_, 1, true, "offline");
  }
  if (ok) {
    mqttClient.publish(topicStatus_, "online", true);
    Serial.print(F("MQTT connected: "));
    Serial.println(topicState_);
    resetReconnectBackoff_();
  }
  return ok;
}

void MqttTelemetry::publishState_(const AirQualityReading &reading, const char *sensorState) {
  JsonDocument doc;
  if (!isnan(reading.temperatureC)) {
    doc["t"] = reading.temperatureC;
  } else {
    doc["t"] = nullptr;
  }
  if (!isnan(reading.humidityPct)) {
    doc["rh"] = reading.humidityPct;
  } else {
    doc["rh"] = nullptr;
  }
  doc["eco2"] = reading.eco2Ppm;
  doc["tvoc"] = reading.tvocPpb;
  doc["aqi"] = reading.aqiUba;
  doc["state"] = sensorState != nullptr ? sensorState : "?";
  doc["ts"] = millis();

  char payload[192];
  const size_t n = serializeJson(doc, payload, sizeof(payload));
  if (n == 0 || n >= sizeof(payload)) {
    return;
  }
  mqttClient.publish(topicState_, payload, true);
}

void MqttTelemetry::tick(uint32_t nowMs, const AirQualityReading &reading,
                         const char *sensorState) {
  const bool wifiUp = wifi_ != nullptr && wifi_->isConnected();
  if (!wifiUp) {
    wasWifiConnected_ = false;
    if (mqttClient.connected()) {
      mqttClient.disconnect();
    }
    return;
  }
  if (!wasWifiConnected_) {
    wasWifiConnected_ = true;
    // Wi‑Fi just came back — retry MQTT soon.
    resetReconnectBackoff_();
  }

  if (cfg_.host[0] == '\0') {
    return;
  }

  if (!mqttClient.connected()) {
    if (nextReconnectMs_ != 0 && nowMs < nextReconnectMs_) {
      return;
    }
    if (!ensureConnected_()) {
      scheduleReconnect_(nowMs, true);
      return;
    }
  }

  mqttClient.loop();

  const uint32_t intervalMs = static_cast<uint32_t>(cfg_.intervalSec) * 1000UL;
  if (lastPublishMs_ != 0 && (nowMs - lastPublishMs_) < intervalMs) {
    return;
  }
  lastPublishMs_ = nowMs;
  publishState_(reading, sensorState);
}

void MqttTelemetry::printStatus() const {
  Serial.print(F("MQTT host="));
  Serial.print(cfg_.host[0] != '\0' ? cfg_.host : "(unset)");
  Serial.print(F(" port="));
  Serial.print(cfg_.port);
  Serial.print(F(" user="));
  Serial.print(cfg_.user[0] != '\0' ? cfg_.user : "(none)");
  Serial.print(F(" pass="));
  Serial.print(cfg_.pass[0] != '\0' ? F("****") : F("(none)"));
  Serial.print(F(" prefix="));
  Serial.print(cfg_.prefix);
  Serial.print(F(" id="));
  Serial.print(deviceIdResolved_);
  Serial.print(F(" interval="));
  Serial.print(cfg_.intervalSec);
  Serial.print(F("s connected="));
  Serial.println(mqttClient.connected() ? F("yes") : F("no"));
  Serial.print(F("  topic state: "));
  Serial.println(topicState_);
  if (!mqttClient.connected() && cfg_.host[0] != '\0') {
    Serial.print(F("  reconnect backoff="));
    Serial.print(reconnectBackoffMs_ / 1000UL);
    Serial.println(F("s (max 300s)"));
  }
}
