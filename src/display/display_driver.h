#pragma once

#include <Arduino.h>

#include "config.h"
#include "history/env_history.h"
#include "sensor/air_quality_sensor.h"

class DisplayDriver {
public:
  bool begin(uint8_t backlightLevel = DEFAULT_BACKLIGHT_LEVEL);
  void setBacklight(uint8_t level);
  void showSplash();
  void updateStatus(const AirQualityReading &reading, const char *sensorState);
  void updateChart(const EnvHistory &history, ChartMetric metric);

private:
  bool initialized_ = false;
  bool statusChromeDrawn_ = false;
  uint8_t backlightLevel_ = DEFAULT_BACKLIGHT_LEVEL;

  void drawStatusChrome_();
  void drawChartBars_(const EnvChartBar *bars, size_t count, ChartMetric metric);
};
