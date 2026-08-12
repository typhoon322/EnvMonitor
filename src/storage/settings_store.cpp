#include "storage/settings_store.h"

#include <Preferences.h>

#include "config.h"
#include "deepseek/deepseek_types.h"

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
    if (view == static_cast<uint8_t>(DisplayView::Chart)) {
      *ctx.displayView = DisplayView::Chart;
    } else if (view == static_cast<uint8_t>(DisplayView::DeepSeek)) {
      *ctx.displayView = DisplayView::DeepSeek;
    } else {
      *ctx.displayView = DisplayView::Status;
    }
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

void SettingsStore::loadDeepSeekConfig(DeepSeekConfig &cfg) const {
  cfg = DeepSeekConfig{};
  if (!opened_) {
    return;
  }

  cfg.keyCount = prefs.getUChar("dsCount", 0);
  if (cfg.keyCount > DEEPSEEK_MAX_KEYS) {
    cfg.keyCount = DEEPSEEK_MAX_KEYS;
  }

  for (uint8_t i = 0; i < cfg.keyCount; ++i) {
    char nameKey[12];
    char apiKeyKey[12];
    snprintf(nameKey, sizeof(nameKey), "dsN%u", i);
    snprintf(apiKeyKey, sizeof(apiKeyKey), "dsK%u", i);
    copyPrefString(cfg.keys[i].name, sizeof(cfg.keys[i].name), nameKey, "");
    copyPrefString(cfg.keys[i].apiKey, sizeof(cfg.keys[i].apiKey), apiKeyKey, "");
  }

  cfg.intervalSec =
      static_cast<uint16_t>(prefs.getUShort("dsIntv", DEEPSEEK_DEFAULT_INTERVAL_SEC));
  if (cfg.intervalSec < DEEPSEEK_INTERVAL_MIN_SEC) {
    cfg.intervalSec = DEEPSEEK_INTERVAL_MIN_SEC;
  }
  if (cfg.intervalSec > DEEPSEEK_INTERVAL_MAX_SEC) {
    cfg.intervalSec = DEEPSEEK_INTERVAL_MAX_SEC;
  }
}

bool SettingsStore::saveDeepSeekConfig(const DeepSeekConfig &cfg) {
  if (!opened_) {
    return false;
  }

  uint8_t count = cfg.keyCount;
  if (count > DEEPSEEK_MAX_KEYS) {
    count = DEEPSEEK_MAX_KEYS;
  }
  prefs.putUChar("dsCount", count);

  for (uint8_t i = 0; i < count; ++i) {
    char nameKey[12];
    char apiKeyKey[12];
    snprintf(nameKey, sizeof(nameKey), "dsN%u", i);
    snprintf(apiKeyKey, sizeof(apiKeyKey), "dsK%u", i);
    prefs.putString(nameKey, cfg.keys[i].name);
    prefs.putString(apiKeyKey, cfg.keys[i].apiKey);
  }

  for (uint8_t i = count; i < DEEPSEEK_MAX_KEYS; ++i) {
    char nameKey[12];
    char apiKeyKey[12];
    snprintf(nameKey, sizeof(nameKey), "dsN%u", i);
    snprintf(apiKeyKey, sizeof(apiKeyKey), "dsK%u", i);
    prefs.remove(nameKey);
    prefs.remove(apiKeyKey);
  }

  prefs.putUShort("dsIntv", cfg.intervalSec);
  return true;
}

void SettingsStore::loadDeepSeekBalances(DeepSeekBalanceEntry *entries, uint8_t maxCount) const {
  if (entries == nullptr || maxCount == 0 || !opened_) {
    return;
  }
  const uint8_t count = maxCount > DEEPSEEK_MAX_KEYS ? DEEPSEEK_MAX_KEYS : maxCount;
  for (uint8_t i = 0; i < count; ++i) {
    char key[12];
    snprintf(key, sizeof(key), "dsBv%u", i);
    if (!prefs.getBool(key, false)) {
      continue;
    }
    entries[i].valid = true;
    entries[i].configured = true;
    snprintf(key, sizeof(key), "dsBa%u", i);
    entries[i].isAvailable = prefs.getBool(key, false);
    snprintf(key, sizeof(key), "dsBc%u", i);
    copyPrefString(entries[i].currency, sizeof(entries[i].currency), key, "CNY");
    snprintf(key, sizeof(key), "dsBt%u", i);
    copyPrefString(entries[i].totalBalance, sizeof(entries[i].totalBalance), key, "0");
    snprintf(key, sizeof(key), "dsBg%u", i);
    copyPrefString(entries[i].grantedBalance, sizeof(entries[i].grantedBalance), key, "0");
    snprintf(key, sizeof(key), "dsBu%u", i);
    copyPrefString(entries[i].toppedUpBalance, sizeof(entries[i].toppedUpBalance), key, "0");
    entries[i].error[0] = '\0';
    entries[i].fetchedAtMs = 0;
  }
}

bool SettingsStore::saveDeepSeekBalances(const DeepSeekBalanceEntry *entries, uint8_t count) {
  if (entries == nullptr || !opened_) {
    return false;
  }
  if (count > DEEPSEEK_MAX_KEYS) {
    count = DEEPSEEK_MAX_KEYS;
  }
  for (uint8_t i = 0; i < count; ++i) {
    char key[12];
    snprintf(key, sizeof(key), "dsBv%u", i);
    prefs.putBool(key, entries[i].valid);
    if (!entries[i].valid) {
      continue;
    }
    snprintf(key, sizeof(key), "dsBa%u", i);
    prefs.putBool(key, entries[i].isAvailable);
    snprintf(key, sizeof(key), "dsBc%u", i);
    prefs.putString(key, entries[i].currency);
    snprintf(key, sizeof(key), "dsBt%u", i);
    prefs.putString(key, entries[i].totalBalance);
    snprintf(key, sizeof(key), "dsBg%u", i);
    prefs.putString(key, entries[i].grantedBalance);
    snprintf(key, sizeof(key), "dsBu%u", i);
    prefs.putString(key, entries[i].toppedUpBalance);
  }
  for (uint8_t i = count; i < DEEPSEEK_MAX_KEYS; ++i) {
    char key[12];
    snprintf(key, sizeof(key), "dsBv%u", i);
    prefs.putBool(key, false);
  }
  return true;
}

uint32_t SettingsStore::loadDeepSeekRefreshEpoch() const {
  if (!opened_) {
    return 0;
  }
  return prefs.getUInt("dsEpoch", 0);
}

bool SettingsStore::saveDeepSeekRefreshEpoch(uint32_t epoch) {
  if (!opened_) {
    return false;
  }
  prefs.putUInt("dsEpoch", epoch);
  return true;
}
