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

void WifiManager::ensureAp_() {
  if (apActive_) {
    return;
  }
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  if (!WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD)) {
    Serial.println(F("WiFi AP start failed."));
    return;
  }
  apActive_ = true;
  Serial.print(F("WiFi AP started: "));
  Serial.print(WIFI_AP_SSID);
  Serial.print(F(" pass="));
  Serial.print(WIFI_AP_PASSWORD);
  Serial.print(F(" ip="));
  Serial.println(WiFi.softAPIP());
}

void WifiManager::begin(SettingsStore *settings) {
  settings_ = settings;
  WiFi.setSleep(false);
  ensureAp_();
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
    Serial.println(F("WiFi: no STA credentials (AP stays on)."));
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
  ensureAp_();
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(false, false);
  delay(50);
  WiFi.begin(ssid_, pass);
}

void WifiManager::tick(uint32_t nowMs) {
  ensureAp_();

  if (!hasCreds_) {
    return;
  }

  const wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED) {
    backoffMs_ = WIFI_RECONNECT_MIN_MS;
    return;
  }

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
  WiFi.disconnect(true, false);
  ensureAp_();
  Serial.println(F("WiFi credentials cleared."));
}

void WifiManager::printStatus() const {
  Serial.print(F("WiFi STA: "));
  if (!hasCreds_) {
    Serial.print(F("no credentials"));
  } else {
    Serial.print(ssid_);
    Serial.print(F(" status="));
    switch (WiFi.status()) {
      case WL_CONNECTED:
        Serial.print(F("connected ip="));
        Serial.print(WiFi.localIP());
        break;
      case WL_NO_SSID_AVAIL:
        Serial.print(F("ssid_not_found"));
        break;
      case WL_CONNECT_FAILED:
        Serial.print(F("connect_failed"));
        break;
      case WL_CONNECTION_LOST:
        Serial.print(F("connection_lost"));
        break;
      case WL_DISCONNECTED:
        Serial.print(F("disconnected"));
        break;
      default:
        Serial.print(static_cast<int>(WiFi.status()));
        break;
    }
  }
  Serial.println();
  Serial.print(F("WiFi AP: "));
  if (apActive_) {
    Serial.print(WIFI_AP_SSID);
    Serial.print(F(" ip="));
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println(F("off"));
  }
}

IPAddress WifiManager::localIP() const {
  return WiFi.localIP();
}

IPAddress WifiManager::apIP() const {
  return WiFi.softAPIP();
}
