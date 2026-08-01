#include <Arduino.h>
#include <esp_task_wdt.h>

#include "cli/serial_cli.h"
#include "config.h"
#include "display/display_driver.h"
#include "history/env_history.h"
#include "sensor/air_quality_sensor.h"
#include "storage/settings_store.h"

AirQualitySensor airQualitySensor;
DisplayDriver displayDriver;
SerialCli serialCli;
EnvHistory envHistory;
SettingsStore settingsStore;

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

SystemContext systemContext;

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
  } else {
    displayDriver.updateStatus(currentReading, airQualitySensor.stateText());
  }
}

void setup() {
  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL);

  serialCli.begin(115200);
  esp_task_wdt_reset();

  Serial.print(F("Board: "));
  Serial.println(F(BOARD_NAME));

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
  serialCli.setContext(systemContext);

  if (settingsStore.load(systemContext)) {
    Serial.println(F("Settings loaded from NVS."));
  }

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

  if (now - lastChartSampleMs >= CHART_SAMPLE_MS) {
    lastChartSampleMs = now;
    if (!isnan(currentReading.temperatureC) && !isnan(currentReading.humidityPct)) {
      envHistory.push(currentReading.temperatureC, currentReading.humidityPct,
                      static_cast<float>(currentReading.eco2Ppm));
    }
  }

  if (now - lastDisplayMs >= DISPLAY_INTERVAL_MS) {
    lastDisplayMs = now;
    refreshDisplay();
  }

  if (now - lastStatusMs >= STATUS_PRINT_INTERVAL_MS) {
    lastStatusMs = now;
    printStatusLine();
  }
}
