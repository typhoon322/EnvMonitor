#pragma once

// Board selection (set by PlatformIO build_flags)
#if defined(BOARD_C3_PRO_MINI)
#include "boards/board_c3_pro_mini.h"
#else
#error "Select board env: esp32-c3-envmonitor"
#endif

// Sensor validation ranges
#define MIN_VALID_TEMP_C -40.0f
#define MAX_VALID_TEMP_C 85.0f
#define MIN_VALID_HUM_PCT 0.0f
#define MAX_VALID_HUM_PCT 100.0f
#define MIN_VALID_ECO2_PPM 400
#define MAX_VALID_ECO2_PPM 65000
#define MIN_VALID_TVOC_PPB 0
#define MAX_VALID_TVOC_PPB 65000
#define MIN_VALID_AQI 1
#define MAX_VALID_AQI 5

#define SENSOR_FILTER_SAMPLES 4
#define MAX_SENSOR_FAULTS 3
#define SENSOR_RECOVER_FAULTS 8
#define SENSOR_RECOVER_COOLDOWN_MS 30000

// Timing (ms)
#define SAMPLE_INTERVAL_MS 1000
#define STATUS_PRINT_INTERVAL_MS 1000
#define DISPLAY_INTERVAL_MS 1000
#define DISPLAY_ANIM_INTERVAL_MS 40
#define DISPLAY_DS_INTERVAL_MS 120
#define CHART_SAMPLE_MS 3000
#define VIEW_BTN_DEBOUNCE_MS 40
#define VIEW_BTN_ACTIVE_HIGH 1
#define DS_BALANCE_ANIM_MS 750

#define DEFAULT_BACKLIGHT_LEVEL 255

// Wi‑Fi / MQTT telemetry (Phase 1)
#define MQTT_DEFAULT_PORT 1883
#define MQTT_DEFAULT_PREFIX "envmonitor"
#define MQTT_DEFAULT_INTERVAL_SEC 10
#define MQTT_INTERVAL_MIN_SEC 5
#define MQTT_INTERVAL_MAX_SEC 300
#define WIFI_RECONNECT_MIN_MS 5000
#define WIFI_RECONNECT_MAX_MS 60000
#define WIFI_AP_SSID "EnvMonitor"
#define WIFI_AP_PASSWORD "envmonitor"
#define NTP_GMT_OFFSET_SEC (8 * 3600)
#define NTP_DAYLIGHT_OFFSET_SEC 0
#define NTP_SERVER_PRIMARY "ntp.aliyun.com"
#define NTP_SERVER_SECONDARY "pool.ntp.org"
#define MQTT_RECONNECT_MIN_MS 5000
#define MQTT_RECONNECT_MAX_MS 300000  // 5 minutes
