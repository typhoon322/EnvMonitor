#include "net/wifi_manager.h"

#include <WiFi.h>

#include "config.h"
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

void WifiManager::begin(SettingsStore *settings) {
  settings_ = settings;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  loadAndConnect_();
}

void WifiManager::loadAndConnect_() {
  char ssid[33];
  char pass[65];
  if (settings_ != nullptr) {
    settings_->loadWifiCredentials(ssid, sizeof(ssid), pass, sizeof(pass));
  } else {
    ssid[0] = '\0';
    pass[0] = '\0';
  }

  hasCreds_ = ssid[0] != '\0';
  copyString(ssid_, sizeof(ssid_), ssid);
  if (!hasCreds_) {
    Serial.println(F("WiFi: no credentials (use 'wifi set <ssid> <pass>')."));
    return;
  }

  backoffMs_ = WIFI_RECONNECT_MIN_MS;
  nextRetryMs_ = 0;
  tryConnect_();
}

void WifiManager::tryConnect_() {
  if (!hasCreds_) {
    return;
  }

  char pass[65];
  if (settings_ != nullptr) {
    char ssid[33];
    settings_->loadWifiCredentials(ssid, sizeof(ssid), pass, sizeof(pass));
  } else {
    pass[0] = '\0';
  }

  Serial.print(F("WiFi STA connecting to "));
  Serial.println(ssid_);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(false, false);
  delay(100);
  WiFi.begin(ssid_, pass);
}

void WifiManager::tick(uint32_t nowMs) {
  if (!hasCreds_) {
    return;
  }

  const wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED) {
    backoffMs_ = WIFI_RECONNECT_MIN_MS;
    return;
  }

  // Still associating — do not restart begin() yet.
  if (st == WL_IDLE_STATUS || st == WL_SCAN_COMPLETED) {
    return;
  }

  if (nowMs < nextRetryMs_) {
    return;
  }
  tryConnect_();
  nextRetryMs_ = nowMs + backoffMs_;
  if (backoffMs_ < WIFI_RECONNECT_MAX_MS) {
    const uint32_t next = backoffMs_ * 2;
    backoffMs_ = next > WIFI_RECONNECT_MAX_MS ? WIFI_RECONNECT_MAX_MS : next;
  }
}

bool WifiManager::isConnected() const {
  return hasCreds_ && WiFi.status() == WL_CONNECTED;
}

bool WifiManager::setCredentials(const char *ssid, const char *pass) {
  if (ssid == nullptr || ssid[0] == '\0') {
    return false;
  }
  if (settings_ != nullptr) {
    settings_->saveWifiCredentials(ssid, pass != nullptr ? pass : "");
  }
  copyString(ssid_, sizeof(ssid_), ssid);
  hasCreds_ = true;
  backoffMs_ = WIFI_RECONNECT_MIN_MS;
  nextRetryMs_ = 0;
  tryConnect_();
  return true;
}

void WifiManager::clearCredentials() {
  if (settings_ != nullptr) {
    settings_->clearWifiCredentials();
  }
  hasCreds_ = false;
  ssid_[0] = '\0';
  WiFi.disconnect(true, true);
  Serial.println(F("WiFi credentials cleared."));
}

void WifiManager::printStatus() const {
  Serial.print(F("WiFi: "));
  if (!hasCreds_) {
    Serial.println(F("no credentials"));
    return;
  }
  Serial.print(ssid_);
  Serial.print(F(" status="));
  switch (WiFi.status()) {
    case WL_CONNECTED:
      Serial.print(F("connected ip="));
      Serial.println(WiFi.localIP());
      break;
    case WL_NO_SSID_AVAIL:
      Serial.println(F("ssid_not_found"));
      break;
    case WL_CONNECT_FAILED:
      Serial.println(F("connect_failed"));
      break;
    case WL_CONNECTION_LOST:
      Serial.println(F("connection_lost"));
      break;
    case WL_DISCONNECTED:
      Serial.println(F("disconnected"));
      break;
    default:
      Serial.println(static_cast<int>(WiFi.status()));
      break;
  }
}

IPAddress WifiManager::localIP() const {
  return WiFi.localIP();
}
