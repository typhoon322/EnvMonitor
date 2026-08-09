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
