#pragma once

#include <Arduino.h>

#define DEEPSEEK_BALANCE_URL "https://api.deepseek.com/user/balance"
#define DEEPSEEK_MAX_KEYS 4
#define DEEPSEEK_KEY_NAME_LEN 16
#define DEEPSEEK_API_KEY_LEN 72
#define DEEPSEEK_BALANCE_LEN 16
#define DEEPSEEK_CURRENCY_LEN 8
#define DEEPSEEK_ERROR_LEN 32
#define DEEPSEEK_DEFAULT_INTERVAL_SEC 180
#define DEEPSEEK_INTERVAL_MIN_SEC 30
#define DEEPSEEK_INTERVAL_MAX_SEC 3600

struct DeepSeekKeyEntry {
  char name[DEEPSEEK_KEY_NAME_LEN] = "";
  char apiKey[DEEPSEEK_API_KEY_LEN] = "";
};

struct DeepSeekBalanceEntry {
  char name[DEEPSEEK_KEY_NAME_LEN] = "";
  bool configured = false;
  bool valid = false;
  bool isAvailable = false;
  char currency[DEEPSEEK_CURRENCY_LEN] = "";
  char totalBalance[DEEPSEEK_BALANCE_LEN] = "";
  char grantedBalance[DEEPSEEK_BALANCE_LEN] = "";
  char toppedUpBalance[DEEPSEEK_BALANCE_LEN] = "";
  char error[DEEPSEEK_ERROR_LEN] = "";
  uint32_t fetchedAtMs = 0;
};

struct DeepSeekConfig {
  DeepSeekKeyEntry keys[DEEPSEEK_MAX_KEYS] = {};
  uint8_t keyCount = 0;
  uint16_t intervalSec = DEEPSEEK_DEFAULT_INTERVAL_SEC;
};
