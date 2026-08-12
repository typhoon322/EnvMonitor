#pragma once

#include <Arduino.h>
#include <IPAddress.h>

#include "config.h"

class SettingsStore;

class WifiManager {
public:
  void begin(SettingsStore *settings);
  void tick(uint32_t nowMs);

  bool isConnected() const;
  bool isApActive() const { return apActive_; }
  bool setCredentials(const char *ssid, const char *pass);
  void clearCredentials();
  void printStatus() const;
  IPAddress localIP() const;
  IPAddress apIP() const;
  const char *staSsid() const { return ssid_; }
  const char *apSsid() const { return WIFI_AP_SSID; }
  bool isTimeSynced() const { return timeSynced_; }

private:
  void loadAndConnect_();
  void tryConnect_();
  void ensureAp_();
  void ensureNtp_();

  SettingsStore *settings_ = nullptr;
  bool hasCreds_ = false;
  bool apActive_ = false;
  bool timeSynced_ = false;
  bool ntpStarted_ = false;
  char ssid_[33] = "";
  uint32_t nextRetryMs_ = 0;
  uint32_t backoffMs_ = WIFI_RECONNECT_MIN_MS;
};
