#include "storage/settings_store.h"

#include <Preferences.h>

#include "config.h"

namespace {
Preferences prefs;

void copyPrefString(char *dest, size_t destLen, const char *key, const char *fallback) {
  if (dest == nullptr || destLen == 0) {
    return;
  }
  const String value = prefs.getString(key, fallback != nullptr ? fallback : "");
  strncpy(dest, value.c_str(), destLen - 1);
  dest[destLen - 1] = '\0';
}
}  // namespace

bool SettingsStore::begin() {
  opened_ = prefs.begin(kNamespace, false);
  return opened_;
}

bool SettingsStore::load(SystemContext &ctx) {
  if (!opened_) {
    return false;
  }

  const uint32_t magic = prefs.getUInt("magic", 0);
  if (magic != kMagicV1) {
    return false;
  }

  if (ctx.displayView != nullptr) {
    const uint8_t view = prefs.getUChar("view", static_cast<uint8_t>(DisplayView::Status));
    *ctx.displayView = view == static_cast<uint8_t>(DisplayView::Chart) ? DisplayView::Chart
                                                                        : DisplayView::Status;
  }

  if (ctx.chartMetric != nullptr) {
    const uint8_t metric = prefs.getUChar("metric", static_cast<uint8_t>(ChartMetric::Temperature));
    if (metric < static_cast<uint8_t>(ChartMetric::Count)) {
      *ctx.chartMetric = static_cast<ChartMetric>(metric);
    }
  }

  if (ctx.chartStep != nullptr && ctx.history != nullptr) {
    const uint8_t step = prefs.getUChar("cStep", static_cast<uint8_t>(ChartStep::S3));
    if (step < static_cast<uint8_t>(ChartStep::Count)) {
      const ChartStep chartStep = static_cast<ChartStep>(step);
      ctx.history->setStep(chartStep);
      *ctx.chartStep = chartStep;
    }
  }

  if (ctx.backlightLevel != nullptr) {
    *ctx.backlightLevel = prefs.getUChar("bl", DEFAULT_BACKLIGHT_LEVEL);
  }

  return true;
}

bool SettingsStore::save(const SystemContext &ctx) {
  if (!opened_) {
    return false;
  }

  prefs.putUInt("magic", kMagicV1);

  if (ctx.displayView != nullptr) {
    prefs.putUChar("view", static_cast<uint8_t>(*ctx.displayView));
  }

  if (ctx.chartMetric != nullptr) {
    prefs.putUChar("metric", static_cast<uint8_t>(*ctx.chartMetric));
  }

  if (ctx.chartStep != nullptr) {
    prefs.putUChar("cStep", static_cast<uint8_t>(*ctx.chartStep));
  }

  if (ctx.backlightLevel != nullptr) {
    prefs.putUChar("bl", *ctx.backlightLevel);
  }

  return true;
}

void SettingsStore::loadWifiCredentials(char *ssid, size_t ssidLen, char *pass,
                                        size_t passLen) const {
  if (!opened_) {
    if (ssid != nullptr && ssidLen > 0) {
      ssid[0] = '\0';
    }
    if (pass != nullptr && passLen > 0) {
      pass[0] = '\0';
    }
    return;
  }
  copyPrefString(ssid, ssidLen, "wifiSsid", "");
  copyPrefString(pass, passLen, "wifiPass", "");
}

bool SettingsStore::saveWifiCredentials(const char *ssid, const char *pass) {
  if (!opened_) {
    return false;
  }
  prefs.putString("wifiSsid", ssid != nullptr ? ssid : "");
  prefs.putString("wifiPass", pass != nullptr ? pass : "");
  return true;
}

void SettingsStore::clearWifiCredentials() {
  if (!opened_) {
    return;
  }
  prefs.remove("wifiSsid");
  prefs.remove("wifiPass");
}

void SettingsStore::loadMqttConfig(MqttConfig &cfg) const {
  cfg = MqttConfig{};
  if (!opened_) {
    return;
  }
  copyPrefString(cfg.host, sizeof(cfg.host), "mqttHost", "");
  cfg.port = static_cast<uint16_t>(prefs.getUShort("mqttPort", MQTT_DEFAULT_PORT));
  copyPrefString(cfg.user, sizeof(cfg.user), "mqttUser", "");
  copyPrefString(cfg.pass, sizeof(cfg.pass), "mqttPass", "");
  copyPrefString(cfg.prefix, sizeof(cfg.prefix), "mqttPrefix", MQTT_DEFAULT_PREFIX);
  if (cfg.prefix[0] == '\0') {
    strncpy(cfg.prefix, MQTT_DEFAULT_PREFIX, sizeof(cfg.prefix) - 1);
    cfg.prefix[sizeof(cfg.prefix) - 1] = '\0';
  }
  copyPrefString(cfg.deviceId, sizeof(cfg.deviceId), "deviceId", "");
  cfg.intervalSec =
      static_cast<uint16_t>(prefs.getUShort("mqttIntv", MQTT_DEFAULT_INTERVAL_SEC));
  if (cfg.intervalSec < MQTT_INTERVAL_MIN_SEC) {
    cfg.intervalSec = MQTT_INTERVAL_MIN_SEC;
  }
  if (cfg.intervalSec > MQTT_INTERVAL_MAX_SEC) {
    cfg.intervalSec = MQTT_INTERVAL_MAX_SEC;
  }
}

bool SettingsStore::saveMqttConfig(const MqttConfig &cfg) {
  if (!opened_) {
    return false;
  }
  prefs.putString("mqttHost", cfg.host);
  prefs.putUShort("mqttPort", cfg.port);
  prefs.putString("mqttUser", cfg.user);
  prefs.putString("mqttPass", cfg.pass);
  prefs.putString("mqttPrefix", cfg.prefix[0] != '\0' ? cfg.prefix : MQTT_DEFAULT_PREFIX);
  prefs.putString("deviceId", cfg.deviceId);
  prefs.putUShort("mqttIntv", cfg.intervalSec);
  return true;
}
