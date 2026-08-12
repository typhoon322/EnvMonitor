#include <Arduino.h>
#include <esp_task_wdt.h>

#include "cli/serial_cli.h"
#include "config.h"
#include "display/display_driver.h"
#include "history/env_history.h"
#include "net/deepseek_monitor.h"
#include "net/mqtt_telemetry.h"
#include "net/web_ui.h"
#include "net/wifi_manager.h"
#include "sensor/air_quality_sensor.h"
#include "storage/settings_store.h"

AirQualitySensor airQualitySensor;
DisplayDriver displayDriver;
SerialCli serialCli;
EnvHistory envHistory;
SettingsStore settingsStore;
WifiManager wifiManager;
MqttTelemetry mqttTelemetry;
DeepSeekMonitor deepSeekMonitor;
WebUi webUi;

AirQualityReading currentReading;
DisplayView displayView = DisplayView::Status;
ChartMetric chartMetric = ChartMetric::Temperature;
ChartStep chartStep = ChartStep::S3;
uint8_t backlightLevel = DEFAULT_BACKLIGHT_LEVEL;

uint32_t lastSampleMs = 0;
uint32_t lastStatusMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastChartSampleMs = 0;
uint8_t consecutiveBadReads = 0;
uint32_t lastSensorRecoverMs = 0;

bool viewBtnStablePressed_ = false;
bool viewBtnLastRaw_ = false;
uint32_t viewBtnLastChangeMs_ = 0;

SystemContext systemContext;

void refreshDisplay();

bool readViewButtonPressed_() {
  const int level = digitalRead(PIN_VIEW_BTN);
#if VIEW_BTN_ACTIVE_HIGH
  return level == HIGH;
#else
  return level == LOW;
#endif
}

void cycleDisplayView_() {
  switch (displayView) {
    case DisplayView::Status:
      displayView = DisplayView::Chart;
      break;
    case DisplayView::Chart:
      displayView = DisplayView::DeepSeek;
      break;
    case DisplayView::DeepSeek:
    default:
      displayView = DisplayView::Status;
      break;
  }
  settingsStore.save(systemContext);
  lastDisplayMs = 0;
  Serial.print(F("View button -> "));
  if (displayView == DisplayView::Chart) {
    Serial.println(F("chart"));
  } else if (displayView == DisplayView::DeepSeek) {
    Serial.println(F("deepseek"));
  } else {
    Serial.println(F("status"));
  }
  refreshDisplay();
}

void pollViewButton_(uint32_t nowMs) {
  const bool rawPressed = readViewButtonPressed_();
  if (rawPressed != viewBtnLastRaw_) {
    viewBtnLastRaw_ = rawPressed;
    viewBtnLastChangeMs_ = nowMs;
    return;
  }
  if (nowMs - viewBtnLastChangeMs_ < VIEW_BTN_DEBOUNCE_MS) {
    return;
  }
  if (rawPressed == viewBtnStablePressed_) {
    return;
  }
  viewBtnStablePressed_ = rawPressed;
  if (viewBtnStablePressed_) {
    cycleDisplayView_();
  }
}

void warmupSensor() {
  for (int i = 0; i < 5; ++i) {
    esp_task_wdt_reset();
    AirQualityReading reading{};
    if (airQualitySensor.read(reading) && airQualitySensor.isValidReading(reading)) {
      currentReading = reading;
    }
    delay(200);
  }
}

void tryRecoverSensor() {
  const uint32_t now = millis();
  if (consecutiveBadReads < SENSOR_RECOVER_FAULTS ||
      now - lastSensorRecoverMs < SENSOR_RECOVER_COOLDOWN_MS) {
    return;
  }

  lastSensorRecoverMs = now;
  consecutiveBadReads = 0;
  if (airQualitySensor.recover()) {
    Serial.println(F("Sensor recover OK."));
  } else {
    Serial.println(F("Sensor recover failed."));
  }
}

void printStatusLine() {
  Serial.print(F("[status] T="));
  if (!isnan(currentReading.temperatureC)) {
    Serial.print(currentReading.temperatureC, 1);
  } else {
    Serial.print(F("N/A"));
  }
  Serial.print(F("C RH="));
  if (!isnan(currentReading.humidityPct)) {
    Serial.print(currentReading.humidityPct, 1);
  } else {
    Serial.print(F("N/A"));
  }
  Serial.print(F("% eCO2="));
  Serial.print(currentReading.eco2Ppm);
  Serial.print(F(" TVOC="));
  Serial.print(currentReading.tvocPpb);
  Serial.print(F(" AQI="));
  Serial.print(currentReading.aqiUba);
  Serial.print(F(" state="));
  Serial.println(airQualitySensor.stateText());
}

void refreshDisplay() {
  if (displayView == DisplayView::Chart) {
    displayDriver.updateChart(envHistory, chartMetric);
  } else if (displayView == DisplayView::DeepSeek) {
    DeepSeekBalanceEntry entries[DEEPSEEK_MAX_KEYS] = {};
    const uint8_t count = deepSeekMonitor.keyCount();
    for (uint8_t i = 0; i < count && i < DEEPSEEK_MAX_KEYS; ++i) {
      const DeepSeekBalanceEntry *entry = deepSeekMonitor.balanceAt(i);
      if (entry != nullptr) {
        entries[i] = *entry;
      }
    }
    displayDriver.updateDeepSeek(entries, count, wifiManager.isConnected(),
                                 deepSeekMonitor.isRefreshing(), deepSeekMonitor.isSpouting(),
                                 deepSeekMonitor.lastRefreshEpoch(), deepSeekMonitor.lastRefreshMs(),
                                 deepSeekMonitor.intervalSec());
  } else {
    displayDriver.updateStatus(currentReading, airQualitySensor.stateText());
  }
}

void setup() {
  esp_task_wdt_init(20, true);
  esp_task_wdt_add(NULL);

  serialCli.begin(115200);
  esp_task_wdt_reset();

  Serial.print(F("Board: "));
  Serial.println(F(BOARD_NAME));
  pinMode(PIN_VIEW_BTN, INPUT);
  viewBtnLastRaw_ = readViewButtonPressed_();
  viewBtnStablePressed_ = viewBtnLastRaw_;
  viewBtnLastChangeMs_ = millis();
  Serial.print(F("View button on GPIO "));
  Serial.println(PIN_VIEW_BTN);
  envHistory.begin();

  if (!settingsStore.begin()) {
    Serial.println(F("WARN: NVS settings store unavailable."));
  }

  systemContext.sensor = &airQualitySensor;
  systemContext.history = &envHistory;
  systemContext.settings = &settingsStore;
  systemContext.reading = &currentReading;
  systemContext.displayView = &displayView;
  systemContext.chartMetric = &chartMetric;
  systemContext.chartStep = &chartStep;
  systemContext.backlightLevel = &backlightLevel;
  systemContext.wifi = &wifiManager;
  systemContext.mqtt = &mqttTelemetry;
  systemContext.deepseek = &deepSeekMonitor;
  serialCli.setContext(systemContext);

  if (settingsStore.load(systemContext)) {
    Serial.println(F("Settings loaded from NVS."));
  }

  wifiManager.begin(&settingsStore);
  mqttTelemetry.begin(&settingsStore, &wifiManager);
  deepSeekMonitor.begin(&settingsStore, &wifiManager);

  WebUiContext webCtx;
  webCtx.settings = &settingsStore;
  webCtx.wifi = &wifiManager;
  webCtx.mqtt = &mqttTelemetry;
  webCtx.deepseek = &deepSeekMonitor;
  webCtx.sensor = &airQualitySensor;
  webCtx.reading = &currentReading;
  webCtx.displayView = &displayView;
  webUi.begin(webCtx);

  if (!displayDriver.begin(backlightLevel)) {
    Serial.println(F("WARN: TFT init failed. Check SPI wiring."));
  } else {
    displayDriver.showSplash();
  }
  if (!airQualitySensor.begin()) {
    Serial.println(F("WARN: air quality sensor init failed. Check I2C wiring."));
  } else {
    warmupSensor();
  }

  esp_task_wdt_reset();
  delay(800);
  refreshDisplay();

  lastSampleMs = millis();
  lastStatusMs = lastSampleMs;
  lastDisplayMs = lastSampleMs;
  lastChartSampleMs = lastSampleMs;
}

void loop() {
  esp_task_wdt_reset();
  serialCli.poll();

  const uint32_t now = millis();
  pollViewButton_(now);

  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;
    AirQualityReading reading{};
    if (airQualitySensor.read(reading) && airQualitySensor.isValidReading(reading)) {
      currentReading = reading;
      consecutiveBadReads = 0;
    } else {
      consecutiveBadReads++;
      tryRecoverSensor();
    }
  }

  wifiManager.tick(now);
  mqttTelemetry.tick(now, currentReading, airQualitySensor.stateText());
  deepSeekMonitor.tick(now, displayView == DisplayView::DeepSeek);
  webUi.tick();

  if (now - lastChartSampleMs >= CHART_SAMPLE_MS) {
    lastChartSampleMs = now;
    if (!isnan(currentReading.temperatureC) && !isnan(currentReading.humidityPct)) {
      envHistory.push(currentReading.temperatureC, currentReading.humidityPct,
                      static_cast<float>(currentReading.eco2Ppm));
    }
  }

  if (now - lastDisplayMs >=
      (displayView == DisplayView::DeepSeek
           ? ((displayDriver.isDeepSeekAnimating() || displayDriver.isDeepSeekSpouting() ||
               deepSeekMonitor.isSpouting())
                  ? DISPLAY_ANIM_INTERVAL_MS
                  : DISPLAY_DS_INTERVAL_MS)
           : DISPLAY_INTERVAL_MS)) {
    lastDisplayMs = now;
    refreshDisplay();
  }

  if (now - lastStatusMs >= STATUS_PRINT_INTERVAL_MS) {
    lastStatusMs = now;
    printStatusLine();
  }
}
