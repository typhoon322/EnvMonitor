#pragma once

#include <Arduino.h>

#include "config.h"
#include "deepseek/deepseek_types.h"
#include "history/env_history.h"
#include "sensor/air_quality_sensor.h"

class DisplayDriver {
public:
  bool begin(uint8_t backlightLevel = DEFAULT_BACKLIGHT_LEVEL);
  void setBacklight(uint8_t level);
  void showSplash();
  void updateStatus(const AirQualityReading &reading, const char *sensorState);
  void updateChart(const EnvHistory &history, ChartMetric metric);
  void updateDeepSeek(const DeepSeekBalanceEntry *entries, size_t count, bool wifiConnected,
                      bool refreshing, uint32_t lastRefreshMs, uint16_t intervalSec);

private:
  bool initialized_ = false;
  bool statusChromeDrawn_ = false;
  bool deepSeekChromeDrawn_ = false;
  uint8_t backlightLevel_ = DEFAULT_BACKLIGHT_LEVEL;

  void drawStatusChrome_();
  void drawDeepSeekChrome_();
  void drawChartBars_(const EnvChartBar *bars, size_t count, ChartMetric metric);
};
