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
  bool setCredentials(const char *ssid, const char *pass);
  void clearCredentials();
  void printStatus() const;
  IPAddress localIP() const;

private:
  void loadAndConnect_();
  void tryConnect_();

  SettingsStore *settings_ = nullptr;
  bool hasCreds_ = false;
  char ssid_[33] = "";
  uint32_t nextRetryMs_ = 0;
  uint32_t backoffMs_ = WIFI_RECONNECT_MIN_MS;
};
