#pragma once

#include <Arduino.h>

#include "history/env_history.h"
#include "sensor/air_quality_sensor.h"

class EnvHistory;
class SettingsStore;

struct SystemContext {
  AirQualitySensor *sensor = nullptr;
  EnvHistory *history = nullptr;
  SettingsStore *settings = nullptr;
  AirQualityReading *reading = nullptr;
  DisplayView *displayView = nullptr;
  ChartMetric *chartMetric = nullptr;
  ChartStep *chartStep = nullptr;
  uint8_t *backlightLevel = nullptr;
};

class SerialCli {
public:
  void begin(unsigned long baud = 115200);
  void setContext(SystemContext context);
  void poll();

private:
  SystemContext ctx_;
  String lineBuffer_;

  void processLine(const String &line);
  void printHelp() const;
  void printStatus() const;
  void persistSettings_();
  bool parseChartStep_(const String &token, ChartStep &out) const;
  bool parseChartMetric_(const String &token, ChartMetric &out) const;
};
