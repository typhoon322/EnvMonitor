#include "net/deepseek_monitor.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "net/wifi_manager.h"
#include "storage/settings_store.h"

namespace {
void copyString(char *dest, size_t destLen, const char *src) {
  if (dest == nullptr || destLen == 0) {
    return;
  }
  strncpy(dest, src != nullptr ? src : "", destLen - 1);
  dest[destLen - 1] = '\0';
}
}  // namespace

void DeepSeekMonitor::resetEntry_(size_t index, const char *name) {
  if (index >= DEEPSEEK_MAX_KEYS) {
    return;
  }
  balances_[index] = DeepSeekBalanceEntry{};
  copyString(balances_[index].name, sizeof(balances_[index].name), name);
  balances_[index].configured = name != nullptr && name[0] != '\0';
}

void DeepSeekMonitor::markError_(size_t index, const char *message) {
  if (index >= DEEPSEEK_MAX_KEYS) {
    return;
  }
  balances_[index].valid = false;
  copyString(balances_[index].error, sizeof(balances_[index].error), message);
}

void DeepSeekMonitor::persistBalances_() {
  if (settings_ == nullptr) {
    return;
  }
  settings_->saveDeepSeekBalances(balances_, cfg_.keyCount);
  hasCached_ = false;
  for (uint8_t i = 0; i < cfg_.keyCount; ++i) {
    if (balances_[i].valid) {
      hasCached_ = true;
      break;
    }
  }
}

void DeepSeekMonitor::restoreBalances_() {
  hasCached_ = false;
  if (settings_ == nullptr || cfg_.keyCount == 0) {
    return;
  }
  for (uint8_t i = 0; i < cfg_.keyCount; ++i) {
    copyString(balances_[i].name, sizeof(balances_[i].name), cfg_.keys[i].name);
    balances_[i].configured = true;
  }
  settings_->loadDeepSeekBalances(balances_, cfg_.keyCount);
  for (uint8_t i = 0; i < cfg_.keyCount; ++i) {
    copyString(balances_[i].name, sizeof(balances_[i].name), cfg_.keys[i].name);
    if (balances_[i].valid) {
      hasCached_ = true;
    }
  }
}

void DeepSeekMonitor::begin(SettingsStore *settings, WifiManager *wifi) {
  settings_ = settings;
  wifi_ = wifi;
  reloadConfig();
}

void DeepSeekMonitor::reloadConfig() {
  if (settings_ != nullptr) {
    settings_->loadDeepSeekConfig(cfg_);
  } else {
    cfg_ = DeepSeekConfig{};
  }

  for (uint8_t i = 0; i < DEEPSEEK_MAX_KEYS; ++i) {
    balances_[i] = DeepSeekBalanceEntry{};
  }
  restoreBalances_();

  lastRefreshMs_ = 0;
  forceRefresh_ = false;
  wasActiveView_ = false;
}

bool DeepSeekMonitor::applyConfig(const DeepSeekConfig &cfg) {
  cfg_ = cfg;
  if (cfg_.intervalSec < DEEPSEEK_INTERVAL_MIN_SEC) {
    cfg_.intervalSec = DEEPSEEK_INTERVAL_MIN_SEC;
  }
  if (cfg_.intervalSec > DEEPSEEK_INTERVAL_MAX_SEC) {
    cfg_.intervalSec = DEEPSEEK_INTERVAL_MAX_SEC;
  }
  bool ok = true;
  if (settings_ != nullptr) {
    ok = settings_->saveDeepSeekConfig(cfg_);
  }
  reloadConfig();
  return ok;
}

bool DeepSeekMonitor::setIntervalSec(uint16_t sec) {
  if (sec < DEEPSEEK_INTERVAL_MIN_SEC || sec > DEEPSEEK_INTERVAL_MAX_SEC) {
    return false;
  }
  cfg_.intervalSec = sec;
  return applyConfig(cfg_);
}

bool DeepSeekMonitor::addKey(const char *name, const char *apiKey) {
  if (name == nullptr || name[0] == '\0' || apiKey == nullptr || apiKey[0] == '\0') {
    return false;
  }
  if (cfg_.keyCount >= DEEPSEEK_MAX_KEYS) {
    return false;
  }

  for (uint8_t i = 0; i < cfg_.keyCount; ++i) {
    if (strncmp(cfg_.keys[i].name, name, DEEPSEEK_KEY_NAME_LEN) == 0) {
      copyString(cfg_.keys[i].apiKey, sizeof(cfg_.keys[i].apiKey), apiKey);
      return applyConfig(cfg_);
    }
  }

  DeepSeekKeyEntry &slot = cfg_.keys[cfg_.keyCount];
  copyString(slot.name, sizeof(slot.name), name);
  copyString(slot.apiKey, sizeof(slot.apiKey), apiKey);
  cfg_.keyCount++;
  return applyConfig(cfg_);
}

bool DeepSeekMonitor::removeKey(const char *nameOrIndex) {
  if (nameOrIndex == nullptr || nameOrIndex[0] == '\0' || cfg_.keyCount == 0) {
    return false;
  }

  int index = -1;
  char *end = nullptr;
  const long value = strtol(nameOrIndex, &end, 10);
  if (end != nameOrIndex && *end == '\0' && value >= 0 &&
      value < static_cast<long>(cfg_.keyCount)) {
    index = static_cast<int>(value);
  } else {
    for (uint8_t i = 0; i < cfg_.keyCount; ++i) {
      if (strncmp(cfg_.keys[i].name, nameOrIndex, DEEPSEEK_KEY_NAME_LEN) == 0) {
        index = static_cast<int>(i);
        break;
      }
    }
  }

  if (index < 0) {
    return false;
  }

  for (uint8_t i = static_cast<uint8_t>(index); i + 1 < cfg_.keyCount; ++i) {
    cfg_.keys[i] = cfg_.keys[i + 1];
  }
  cfg_.keys[cfg_.keyCount - 1] = DeepSeekKeyEntry{};
  cfg_.keyCount--;
  return applyConfig(cfg_);
}

void DeepSeekMonitor::listKeys() const {
  if (cfg_.keyCount == 0) {
    Serial.println(F("No DeepSeek keys configured."));
    return;
  }
  for (uint8_t i = 0; i < cfg_.keyCount; ++i) {
    Serial.print(i);
    Serial.print(F(": "));
    Serial.print(cfg_.keys[i].name);
    Serial.print(F(" key="));
    const char *key = cfg_.keys[i].apiKey;
    const size_t len = strlen(key);
    if (len <= 8) {
      Serial.println(F("****"));
    } else {
      for (size_t j = 0; j < 4; ++j) {
        Serial.print(key[j]);
      }
      Serial.print(F("..."));
      for (size_t j = len - 4; j < len; ++j) {
        Serial.print(key[j]);
      }
      Serial.println();
    }
  }
}

void DeepSeekMonitor::requestRefresh() {
  forceRefresh_ = true;
}

const DeepSeekBalanceEntry *DeepSeekMonitor::balanceAt(size_t index) const {
  if (index >= cfg_.keyCount) {
    return nullptr;
  }
  return &balances_[index];
}

bool DeepSeekMonitor::fetchOne_(size_t index) {
  if (index >= cfg_.keyCount) {
    return false;
  }

  const DeepSeekKeyEntry &key = cfg_.keys[index];
  const bool keepName = true;
  char savedName[DEEPSEEK_KEY_NAME_LEN];
  copyString(savedName, sizeof(savedName), key.name);
  // Keep cached values until success; only clear error on attempt.
  balances_[index].configured = true;
  copyString(balances_[index].name, sizeof(balances_[index].name), savedName);
  balances_[index].error[0] = '\0';
  (void)keepName;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15);

  HTTPClient http;
  http.setTimeout(15000);
  if (!http.begin(client, DEEPSEEK_BALANCE_URL)) {
    markError_(index, "HTTP init failed");
    return false;
  }

  String authHeader = F("Bearer ");
  authHeader += key.apiKey;
  http.addHeader(F("Authorization"), authHeader);
  http.addHeader(F("Accept"), F("application/json"));

  esp_task_wdt_reset();
  const int code = http.GET();
  esp_task_wdt_reset();

  if (code != HTTP_CODE_OK) {
    char msg[DEEPSEEK_ERROR_LEN];
    snprintf(msg, sizeof(msg), "HTTP %d", code);
    markError_(index, msg);
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    markError_(index, "JSON parse error");
    return false;
  }

  balances_[index].valid = true;
  balances_[index].isAvailable = doc["is_available"] | false;
  balances_[index].fetchedAtMs = millis();
  balances_[index].error[0] = '\0';

  JsonArray infos = doc["balance_infos"].as<JsonArray>();
  if (!infos.isNull() && infos.size() > 0) {
    JsonObject item = infos[0];
    copyString(balances_[index].currency, sizeof(balances_[index].currency),
               item["currency"] | "CNY");
    copyString(balances_[index].totalBalance, sizeof(balances_[index].totalBalance),
               item["total_balance"] | "0");
    copyString(balances_[index].grantedBalance, sizeof(balances_[index].grantedBalance),
               item["granted_balance"] | "0");
    copyString(balances_[index].toppedUpBalance, sizeof(balances_[index].toppedUpBalance),
               item["topped_up_balance"] | "0");
  } else {
    copyString(balances_[index].currency, sizeof(balances_[index].currency), "CNY");
    copyString(balances_[index].totalBalance, sizeof(balances_[index].totalBalance), "0");
  }

  return true;
}

void DeepSeekMonitor::refreshAll_() {
  if (cfg_.keyCount == 0 || refreshing_) {
    return;
  }
  if (wifi_ == nullptr || !wifi_->isConnected()) {
    Serial.println(F("[deepseek] Skip refresh: WiFi down"));
    return;
  }

  refreshing_ = true;
  forceRefresh_ = false;
  Serial.println(F("[deepseek] Refreshing balances..."));

  bool anyOk = false;
  for (uint8_t i = 0; i < cfg_.keyCount; ++i) {
    if (fetchOne_(i)) {
      anyOk = true;
    }
    esp_task_wdt_reset();
  }

  lastRefreshMs_ = millis();
  if (anyOk) {
    persistBalances_();
  }
  refreshing_ = false;
  Serial.println(F("[deepseek] Refresh complete."));
}

void DeepSeekMonitor::tick(uint32_t nowMs, bool activeView) {
  if (forceRefresh_) {
    refreshAll_();
    wasActiveView_ = activeView;
    return;
  }

  if (!activeView || cfg_.keyCount == 0) {
    wasActiveView_ = activeView;
    return;
  }

  if (wifi_ == nullptr || !wifi_->isConnected()) {
    wasActiveView_ = activeView;
    return;
  }

  const bool enteredView = activeView && !wasActiveView_;
  wasActiveView_ = activeView;

  const uint32_t intervalMs = static_cast<uint32_t>(cfg_.intervalSec) * 1000UL;
  const bool due =
      enteredView || lastRefreshMs_ == 0 || nowMs - lastRefreshMs_ >= intervalMs;
  if (due) {
    refreshAll_();
  }
}

void DeepSeekMonitor::printStatus() const {
  Serial.print(F("DeepSeek keys: "));
  Serial.println(cfg_.keyCount);
  Serial.print(F("Refresh interval: "));
  Serial.print(cfg_.intervalSec);
  Serial.println(F("s (auto only on DS view)"));
  Serial.print(F("Cached: "));
  Serial.println(hasCached_ ? F("yes") : F("no"));
  if (lastRefreshMs_ > 0) {
    Serial.print(F("Last refresh: "));
    Serial.print((millis() - lastRefreshMs_) / 1000UL);
    Serial.println(F("s ago"));
  } else {
    Serial.println(F("Last refresh: never this boot"));
  }
  if (refreshing_) {
    Serial.println(F("Status: refreshing"));
  }
  for (uint8_t i = 0; i < cfg_.keyCount; ++i) {
    const DeepSeekBalanceEntry &b = balances_[i];
    Serial.print(F("  "));
    Serial.print(b.name);
    Serial.print(F(": "));
    if (b.valid) {
      Serial.print(b.currency);
      Serial.print(F(" "));
      Serial.print(b.totalBalance);
      Serial.print(F(" avail="));
      Serial.println(b.isAvailable ? F("yes") : F("no"));
    } else if (b.error[0] != '\0') {
      Serial.println(b.error);
    } else {
      Serial.println(F("pending"));
    }
  }
}
