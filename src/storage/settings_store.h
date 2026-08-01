#pragma once

#include <Arduino.h>

#include "cli/serial_cli.h"

class SettingsStore {
public:
  bool begin();
  bool load(SystemContext &ctx);
  bool save(const SystemContext &ctx);

private:
  static constexpr uint32_t kMagicV1 = 0x454E5631;  // "ENV1"
  static constexpr const char *kNamespace = "envmon";

  bool opened_ = false;
};
