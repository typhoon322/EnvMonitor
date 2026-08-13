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
                      bool refreshing, bool spouting, uint32_t lastRefreshEpoch,
                      uint32_t lastRefreshMs, uint16_t intervalSec);
  bool isDeepSeekAnimating() const { return dsAnimating_; }
  bool isDeepSeekSpouting() const { return dsSpouting_; }

private:
  static constexpr int kDsBalCx = 160;
  static constexpr int kDsBalCy = 86;
  static constexpr int kDsBalFont = 7;  // one step below Font 8
  static constexpr int kDsWhaleY = 142;
  static constexpr int kDsWhaleH = 32;  // matches deepseek_whale_bmp.h
  static constexpr int kDsWhaleW = 52;
  static constexpr int kDsLanePad = 2;

  bool initialized_ = false;
  bool statusChromeDrawn_ = false;
  bool deepSeekChromeDrawn_ = false;
  uint8_t backlightLevel_ = DEFAULT_BACKLIGHT_LEVEL;

  bool dsCacheValid_ = false;
  bool dsWifi_ = false;
  bool dsRefreshing_ = false;
  bool dsHasKey_ = false;
  bool dsValid_ = false;
  bool dsAvailable_ = false;
  bool dsLowBalance_ = false;
  uint8_t dsKeyCount_ = 0;
  uint16_t dsIntervalSec_ = 0;
  uint32_t dsEpoch_ = 0;
  int16_t dsRemainSec_ = -2;
  int16_t dsWhaleX_ = -1;
  int16_t dsSpoutLeft_ = -1;
  int16_t dsSpoutW_ = 0;
  bool dsWhaleRight_ = true;
  bool dsWhaleSpout_ = false;
  bool dsSpouting_ = false;
  uint8_t dsSpoutFrame_ = 255;
  uint32_t dsSpoutUntilMs_ = 0;
  char dsBalance_[DEEPSEEK_BALANCE_LEN] = "";

  bool dsAnimating_ = false;
  bool dsAnimUp_ = true;
  uint32_t dsAnimStartMs_ = 0;
  uint16_t dsAnimColor_ = 0;
  char dsAnimFrom_[DEEPSEEK_BALANCE_LEN] = "";
  char dsAnimTo_[DEEPSEEK_BALANCE_LEN] = "";

  void drawStatusChrome_();
  void drawDeepSeekChrome_();
  void resetDeepSeekCache_();
  void drawChartBars_(const EnvChartBar *bars, size_t count, ChartMetric metric);
  void startBalanceAnim_(const char *from, const char *to, uint16_t color);
  void drawRollingBalance_(float t);
  void drawStaticBalance_(const char *balance, uint16_t color);
  void clearBalanceBand_();
  void clearSpoutResidue_();
  void drawDeepSeekMeta_(bool lowBalance, uint32_t lastRefreshEpoch, size_t count);
  void drawCountdownText_(int remainSec);
  void drawWhaleLane_(uint32_t lastRefreshMs, uint16_t intervalSec, bool celebrate);
  void drawWhaleSprite_(int x, int y, bool facingRight, bool spout, uint8_t spoutFrame);
  void drawSpoutPlume_(int bx, int by, uint8_t frame);
  static float whalePathX_(float progress);
};
