#pragma once

#include <Arduino.h>

#include "history/env_history.h"
#include "sensor/air_quality_sensor.h"

class SettingsStore;
class WifiManager;
class MqttTelemetry;
class DeepSeekMonitor;

struct WebUiContext {
  SettingsStore *settings = nullptr;
  WifiManager *wifi = nullptr;
  MqttTelemetry *mqtt = nullptr;
  DeepSeekMonitor *deepseek = nullptr;
  AirQualitySensor *sensor = nullptr;
  AirQualityReading *reading = nullptr;
  DisplayView *displayView = nullptr;
};

class WebUi {
public:
  void begin(const WebUiContext &ctx);
  void tick();

private:
  WebUiContext ctx_{};
  bool started_ = false;

  void handleRoot_();
  void handleStatus_();
  void handleConfig_();
  void handleWifi_();
  void handleMqtt_();
  void handleDeepSeek_();
  void handleView_();
  void sendJson_(int code, const String &body);
};
