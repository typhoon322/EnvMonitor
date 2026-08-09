#pragma once

#include <Arduino.h>

#include "config.h"
#include "sensor/air_quality_sensor.h"
#include "storage/settings_store.h"

class WifiManager;

class MqttTelemetry {
public:
  void begin(SettingsStore *settings, WifiManager *wifi);
  void tick(uint32_t nowMs, const AirQualityReading &reading, const char *sensorState);

  void reloadConfig();
  bool applyConfig(const MqttConfig &cfg);
  bool setIntervalSec(uint16_t sec);
  uint16_t intervalSec() const { return cfg_.intervalSec; }
  void printStatus() const;

private:
  void resolveDeviceId_();
  void buildTopics_();
  bool ensureConnected_();
  void scheduleReconnect_(uint32_t nowMs, bool failed);
  void resetReconnectBackoff_();
  void publishState_(const AirQualityReading &reading, const char *sensorState);

  SettingsStore *settings_ = nullptr;
  WifiManager *wifi_ = nullptr;
  MqttConfig cfg_{};
  char deviceIdResolved_[24] = "";
  char topicState_[96] = "";
  char topicStatus_[96] = "";
  uint32_t lastPublishMs_ = 0;
  uint32_t nextReconnectMs_ = 0;
  uint32_t reconnectBackoffMs_ = MQTT_RECONNECT_MIN_MS;
  bool wasWifiConnected_ = false;
};
