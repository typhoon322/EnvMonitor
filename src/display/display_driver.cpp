#include "display/display_driver.h"

#include <TFT_eSPI.h>
#include <math.h>

#include "config.h"

namespace {
// ST7789 1.47" panel used in landscape (rotation 1)
constexpr int kScreenW = 320;
constexpr int kScreenH = 172;
constexpr int kChartTop = 30;
constexpr int kChartBottom = 164;
constexpr int kChartLeft = 40;
constexpr int kChartRight = 316;

TFT_eSPI tft;

uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

const uint16_t kBg = TFT_BLACK;
const uint16_t kText = TFT_WHITE;
const uint16_t kAccent = TFT_CYAN;
const uint16_t kWarn = TFT_YELLOW;
const uint16_t kBad = TFT_RED;
const uint16_t kGood = TFT_GREEN;
const uint16_t kChartLine = TFT_GREEN;
const uint16_t kChartGrid = TFT_DARKGREY;

void clearField(int x, int y, int w, int h) {
  tft.fillRect(x, y, w, h, kBg);
}

int valueToY(float value, float yMin, float yMax) {
  if (yMax <= yMin) {
    return (kChartTop + kChartBottom) / 2;
  }
  const float t = (value - yMin) / (yMax - yMin);
  const int y = kChartBottom - static_cast<int>(t * (kChartBottom - kChartTop));
  if (y < kChartTop) {
    return kChartTop;
  }
  if (y > kChartBottom) {
    return kChartBottom;
  }
  return y;
}

void computeRange(const EnvChartBar *bars, size_t count, ChartMetric metric, float &yMin,
                  float &yMax) {
  yMin = 1e9f;
  yMax = -1e9f;
  for (size_t i = 0; i < count; ++i) {
    float high = 0.0f;
    float low = 0.0f;
    float close = 0.0f;
    EnvHistory::barValues(bars[i], metric, high, low, close);
    if (low < yMin) {
      yMin = low;
    }
    if (high > yMax) {
      yMax = high;
    }
    if (close < yMin) {
      yMin = close;
    }
    if (close > yMax) {
      yMax = close;
    }
  }
  if (yMax - yMin < 0.5f) {
    const float mid = (yMin + yMax) * 0.5f;
    yMin = mid - 0.5f;
    yMax = mid + 0.5f;
  } else {
    const float pad = (yMax - yMin) * 0.08f;
    yMin -= pad;
    yMax += pad;
  }
}

const char *metricLabel(ChartMetric metric) {
  switch (metric) {
    case ChartMetric::Humidity:
      return "Humidity %";
    case ChartMetric::Eco2:
      return "eCO2 ppm";
    case ChartMetric::Temperature:
    default:
      return "Temp C";
  }
}

uint16_t aqiColor(uint8_t aqi) {
  switch (aqi) {
    case 1:
      return kGood;
    case 2:
      return color565(128, 255, 0);
    case 3:
      return kWarn;
    case 4:
      return color565(255, 128, 0);
    case 5:
      return kBad;
    default:
      return kText;
  }
}
}  // namespace

bool DisplayDriver::begin(uint8_t backlightLevel) {
  // TFT_eSPI reads pins + ST7789 config from the sketch-level tft_setup.h
  // (auto-included); it initialises the SPI bus and display reset.
  tft.init();
  tft.setRotation(1);  // 320x172 landscape
  tft.fillScreen(kBg);
  setBacklight(backlightLevel);
  initialized_ = true;
  statusChromeDrawn_ = false;
  return true;
}

void DisplayDriver::setBacklight(uint8_t level) {
  backlightLevel_ = level;
  pinMode(PIN_TFT_BL, OUTPUT);
  if (level == 0) {
    digitalWrite(PIN_TFT_BL, LOW);
  } else {
    digitalWrite(PIN_TFT_BL, HIGH);
  }
}

void DisplayDriver::showSplash() {
  if (!initialized_) {
    return;
  }
  statusChromeDrawn_ = false;
  tft.fillScreen(kBg);
  tft.setTextColor(kAccent);
  tft.setTextSize(3);
  tft.setCursor(52, 58);
  tft.print(F("EnvMonitor"));
  tft.setTextSize(2);
  tft.setTextColor(kText);
  tft.setCursor(100, 98);
  tft.print(F("Starting..."));
}

void DisplayDriver::drawStatusChrome_() {
  tft.fillScreen(kBg);

  tft.setTextColor(kAccent);
  tft.setTextSize(2);
  tft.setCursor(8, 6);
  tft.print(F("EnvMonitor"));

  tft.setTextSize(2);
  tft.setTextColor(kText);
  tft.setCursor(8, 78);
  tft.print(F("eCO2:"));
  tft.setCursor(8, 100);
  tft.print(F("TVOC:"));
  tft.setCursor(8, 122);
  tft.print(F("AQI:"));
}

void DisplayDriver::updateStatus(const AirQualityReading &reading, const char *sensorState) {
  if (!initialized_) {
    return;
  }

  if (!statusChromeDrawn_) {
    drawStatusChrome_();
    statusChromeDrawn_ = true;
  }

  // Sensor state (top-right)
  clearField(200, 6, 112, 16);
  tft.setTextSize(1);
  tft.setTextColor(kText);
  tft.setCursor(252, 12);
  if (sensorState != nullptr) {
    tft.print(sensorState);
  } else {
    tft.print(F("?"));
  }

  // Temperature
  clearField(8, 36, 120, 36);
  tft.setTextSize(4);
  tft.setTextColor(kGood);
  tft.setCursor(8, 36);
  if (!isnan(reading.temperatureC)) {
    tft.print(reading.temperatureC, 1);
  } else {
    tft.print(F("--"));
  }
  tft.setTextSize(2);
  tft.print(F(" C"));

  // Humidity
  clearField(140, 36, 172, 36);
  tft.setTextSize(3);
  tft.setTextColor(kAccent);
  tft.setCursor(140, 42);
  if (!isnan(reading.humidityPct)) {
    tft.print(reading.humidityPct, 1);
  } else {
    tft.print(F("--"));
  }
  tft.setTextSize(2);
  tft.print(F(" %RH"));

  // eCO2 value
  clearField(80, 78, 232, 20);
  tft.setTextSize(2);
  tft.setTextColor(kWarn);
  tft.setCursor(80, 78);
  tft.print(reading.eco2Ppm);
  tft.setTextColor(kText);
  tft.print(F(" ppm"));

  // TVOC value
  clearField(80, 100, 232, 20);
  tft.setTextColor(kText);
  tft.setCursor(80, 100);
  tft.print(reading.tvocPpb);
  tft.print(F(" ppb"));

  // AQI value
  clearField(80, 122, 232, 20);
  tft.setTextColor(aqiColor(reading.aqiUba));
  tft.setCursor(80, 122);
  tft.print(reading.aqiUba);
  tft.setTextColor(kText);
  tft.print(F(" / 5"));

  // Gas sensor footer
  clearField(8, 146, 304, 20);
  tft.setCursor(8, 146);
  if (reading.ens160Ready) {
    tft.setTextColor(kGood);
    tft.print(F("Gas sensor: operating"));
  } else {
    tft.setTextColor(kWarn);
    tft.print(F("Gas sensor: warming up"));
  }
}

void DisplayDriver::drawChartBars_(const EnvChartBar *bars, size_t count, ChartMetric metric) {
  if (count == 0) {
    tft.setTextSize(2);
    tft.setTextColor(kText);
    tft.setCursor(70, 74);
    tft.print(F("Collecting data..."));
    return;
  }

  float yMin = 0.0f;
  float yMax = 0.0f;
  computeRange(bars, count, metric, yMin, yMax);

  tft.drawLine(kChartLeft, kChartTop, kChartLeft, kChartBottom, kChartGrid);
  tft.drawLine(kChartLeft, kChartBottom, kChartRight, kChartBottom, kChartGrid);

  tft.setTextSize(1);
  tft.setTextColor(kText);
  tft.setCursor(4, kChartTop);
  tft.print(yMax, 1);
  tft.setCursor(4, kChartBottom - 8);
  tft.print(yMin, 1);

  const int width = kChartRight - kChartLeft;
  int prevX = -1;
  int prevY = -1;
  for (size_t i = 0; i < count; ++i) {
    float high = 0.0f;
    float low = 0.0f;
    float close = 0.0f;
    EnvHistory::barValues(bars[i], metric, high, low, close);
    const int x = kChartLeft + static_cast<int>((i * width) / (count > 1 ? count - 1 : 1));
    const int yClose = valueToY(close, yMin, yMax);
    const int yHi = valueToY(high, yMin, yMax);
    const int yLo = valueToY(low, yMin, yMax);
    tft.drawLine(x, yHi, x, yLo, kChartGrid);
    tft.drawPixel(x, yClose, kChartLine);
    if (prevX >= 0) {
      tft.drawLine(prevX, prevY, x, yClose, kChartLine);
    }
    prevX = x;
    prevY = yClose;
  }
}

void DisplayDriver::updateChart(const EnvHistory &history, ChartMetric metric) {
  if (!initialized_) {
    return;
  }

  statusChromeDrawn_ = false;
  deepSeekChromeDrawn_ = false;

  EnvChartBar bars[128];
  const size_t count = history.copyBars(bars, 128);

  tft.fillScreen(kBg);
  tft.setTextColor(kAccent);
  tft.setTextSize(2);
  tft.setCursor(8, 6);
  tft.print(F("Chart "));
  tft.print(history.stepLabel());
  tft.print(F("  "));
  tft.print(metricLabel(metric));

  drawChartBars_(bars, count, metric);
}

void DisplayDriver::drawDeepSeekChrome_() {
  tft.fillScreen(kBg);
  deepSeekChromeDrawn_ = true;
  statusChromeDrawn_ = false;

  tft.setTextColor(kAccent);
  tft.setTextSize(2);
  tft.setCursor(8, 6);
  tft.print(F("DeepSeek"));
}

void DisplayDriver::updateDeepSeek(const DeepSeekBalanceEntry *entries, size_t count,
                                   bool wifiConnected, bool refreshing, uint32_t lastRefreshMs,
                                   uint16_t intervalSec) {
  if (!initialized_) {
    return;
  }

  if (!deepSeekChromeDrawn_) {
    drawDeepSeekChrome_();
  }

  // Wi-Fi indicator (top-right)
  clearField(220, 6, 92, 16);
  tft.setTextSize(1);
  tft.setTextColor(wifiConnected ? kGood : kBad);
  tft.setCursor(228, 12);
  tft.print(wifiConnected ? F("WiFi OK") : F("No WiFi"));

  if (count == 0) {
    clearField(8, 40, 304, 100);
    tft.setTextSize(2);
    tft.setTextColor(kWarn);
    tft.setCursor(8, 48);
    tft.println(F("No API Key"));
    tft.setTextSize(1);
    tft.setTextColor(kText);
    tft.setCursor(8, 78);
    tft.println(F("Configure via WebUI or:"));
    tft.setCursor(8, 92);
    tft.println(F("ds add <name> <key>"));
    tft.setCursor(8, 110);
    tft.println(F("AP: EnvMonitor"));
    tft.setCursor(8, 124);
    tft.println(F("open http://192.168.4.1"));
    return;
  }

  constexpr int kRowTop = 34;
  constexpr int kRowH = 28;
  for (size_t i = 0; i < DEEPSEEK_MAX_KEYS; ++i) {
    const int y = kRowTop + static_cast<int>(i) * kRowH;
    clearField(8, y, 304, kRowH - 2);

    if (i >= count) {
      continue;
    }

    const DeepSeekBalanceEntry &entry = entries[i];
    tft.setTextSize(2);
    tft.setTextColor(kAccent);
    tft.setCursor(8, y + 2);
    tft.print(entry.name);

    tft.setTextSize(1);
    if (!entry.valid && entry.error[0] != '\0') {
      tft.setTextColor(kBad);
      tft.setCursor(8, y + 18);
      tft.print(entry.error);
      continue;
    }

    if (!entry.valid) {
      tft.setTextColor(kWarn);
      tft.setCursor(8, y + 18);
      tft.print(refreshing ? F("Fetching...") : F("Pending..."));
      continue;
    }

    tft.setTextColor(kText);
    tft.setCursor(120, y + 6);
    tft.print(entry.currency);
    tft.print(F(" "));
    tft.setTextSize(2);
    tft.setTextColor(entry.isAvailable ? kGood : kWarn);
    tft.print(entry.totalBalance);

    tft.setTextSize(1);
    tft.setTextColor(kText);
    tft.setCursor(250, y + 10);
    tft.print(entry.isAvailable ? F("OK") : F("N/A"));
  }

  clearField(8, 150, 304, 18);
  tft.setTextSize(1);
  tft.setTextColor(kText);
  tft.setCursor(8, 154);
  if (refreshing) {
    tft.print(F("Updating..."));
  } else if (lastRefreshMs > 0) {
    const uint32_t ageSec = (millis() - lastRefreshMs) / 1000UL;
    tft.print(F("Updated "));
    tft.print(ageSec);
    tft.print(F("s ago  every "));
    tft.print(intervalSec);
    tft.print(F("s"));
  } else if (wifiConnected) {
    tft.print(F("Waiting for first fetch..."));
  } else {
    tft.print(F("Connect WiFi to fetch balance"));
  }
}
