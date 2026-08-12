#pragma once

#include <Arduino.h>

#include "cli/serial_cli.h"
#include "config.h"
#include "deepseek/deepseek_types.h"

struct MqttConfig {
  char host[64] = "";
  uint16_t port = MQTT_DEFAULT_PORT;
  char user[32] = "";
  char pass[64] = "";
  char prefix[32] = MQTT_DEFAULT_PREFIX;
  char deviceId[24] = "";  // empty => derive from MAC at runtime
  uint16_t intervalSec = MQTT_DEFAULT_INTERVAL_SEC;
};

class SettingsStore {
public:
  bool begin();
  bool load(SystemContext &ctx);
  bool save(const SystemContext &ctx);

  void loadWifiCredentials(char *ssid, size_t ssidLen, char *pass, size_t passLen) const;
  bool saveWifiCredentials(const char *ssid, const char *pass);
  void clearWifiCredentials();

  void loadMqttConfig(MqttConfig &cfg) const;
  bool saveMqttConfig(const MqttConfig &cfg);

  void loadDeepSeekConfig(DeepSeekConfig &cfg) const;
  bool saveDeepSeekConfig(const DeepSeekConfig &cfg);

  void loadDeepSeekBalances(DeepSeekBalanceEntry *entries, uint8_t maxCount) const;
  bool saveDeepSeekBalances(const DeepSeekBalanceEntry *entries, uint8_t count);
  uint32_t loadDeepSeekRefreshEpoch() const;
  bool saveDeepSeekRefreshEpoch(uint32_t epoch);

private:
  static constexpr uint32_t kMagicV1 = 0x454E5631;  // "ENV1"
  static constexpr const char *kNamespace = "envmon";

  bool opened_ = false;
};
