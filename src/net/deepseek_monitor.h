#pragma once

#include <Arduino.h>

#include "deepseek/deepseek_types.h"

class SettingsStore;
class WifiManager;

class DeepSeekMonitor {
public:
  void begin(SettingsStore *settings, WifiManager *wifi);
  // activeView: only auto-poll when TFT is on DeepSeek view.
  void tick(uint32_t nowMs, bool activeView);

  void reloadConfig();
  bool applyConfig(const DeepSeekConfig &cfg);
  uint16_t intervalSec() const { return cfg_.intervalSec; }
  bool setIntervalSec(uint16_t sec);
  bool addKey(const char *name, const char *apiKey);
  bool removeKey(const char *nameOrIndex);
  void listKeys() const;
  void requestRefresh();
  void printStatus() const;

  uint8_t keyCount() const { return cfg_.keyCount; }
  const DeepSeekBalanceEntry *balanceAt(size_t index) const;
  size_t balanceCount() const { return cfg_.keyCount; }
  bool isRefreshing() const { return refreshing_; }
  bool isSpouting() const { return spoutHold_; }
  uint32_t lastRefreshMs() const { return lastRefreshMs_; }
  uint32_t lastRefreshEpoch() const { return lastRefreshEpoch_; }
  bool hasCachedBalances() const { return hasCached_; }

private:
  static constexpr uint16_t kHttpTimeoutMs = 8000;
  static constexpr uint32_t kSpoutHoldMs = 1800;

  bool fetchOne_(size_t index);
  void markError_(size_t index, const char *message);
  void resetEntry_(size_t index, const char *name);
  void persistBalances_();
  void restoreBalances_();
  void startRefresh_();
  void finishRefresh_();
  void beginSpoutThenRefresh_();

  SettingsStore *settings_ = nullptr;
  WifiManager *wifi_ = nullptr;
  DeepSeekConfig cfg_{};
  DeepSeekBalanceEntry balances_[DEEPSEEK_MAX_KEYS] = {};
  uint32_t lastRefreshMs_ = 0;
  uint32_t lastRefreshEpoch_ = 0;
  bool forceRefresh_ = false;
  bool refreshing_ = false;
  bool spoutHold_ = false;
  uint32_t spoutHoldUntilMs_ = 0;
  bool refreshAnyOk_ = false;
  uint8_t refreshIndex_ = 0;
  bool hasCached_ = false;
  bool wasActiveView_ = false;
};
