#include "display/display_driver.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <math.h>

#include "config.h"

namespace {
constexpr int kScreenW = 320;
constexpr int kScreenH = 240;
constexpr int kChartTop = 36;
constexpr int kChartBottom = 220;
constexpr int kChartLeft = 40;
constexpr int kChartRight = 310;

Adafruit_ILI9341 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

const uint16_t kBg = ILI9341_BLACK;
const uint16_t kText = ILI9341_WHITE;
const uint16_t kAccent = ILI9341_CYAN;
const uint16_t kWarn = ILI9341_YELLOW;
const uint16_t kBad = ILI9341_RED;
const uint16_t kGood = ILI9341_GREEN;
const uint16_t kChartLine = ILI9341_GREEN;
const uint16_t kChartGrid = ILI9341_DARKGREY;

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
  pinMode(PIN_TFT_BL, OUTPUT);
  setBacklight(backlightLevel);

  SPI.begin(PIN_TFT_SCK, -1, PIN_TFT_MOSI, PIN_TFT_CS);
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(kBg);
  initialized_ = true;
  return true;
}

void DisplayDriver::setBacklight(uint8_t level) {
  backlightLevel_ = level;
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
  tft.fillScreen(kBg);
  tft.setTextColor(kAccent);
  tft.setTextSize(3);
  tft.setCursor(40, 90);
  tft.print(F("EnvMonitor"));
  tft.setTextSize(2);
  tft.setTextColor(kText);
  tft.setCursor(70, 130);
  tft.print(F("Starting..."));
}

void DisplayDriver::updateStatus(const AirQualityReading &reading, const char *sensorState) {
  if (!initialized_) {
    return;
  }

  tft.fillScreen(kBg);
  tft.setTextColor(kAccent);
  tft.setTextSize(2);
  tft.setCursor(8, 8);
  tft.print(F("EnvMonitor"));

  tft.setTextSize(1);
  tft.setTextColor(kText);
  tft.setCursor(220, 12);
  if (sensorState != nullptr) {
    tft.print(sensorState);
  } else {
    tft.print(F("?"));
  }

  tft.setTextSize(4);
  tft.setTextColor(kGood);
  tft.setCursor(8, 40);
  if (!isnan(reading.temperatureC)) {
    tft.print(reading.temperatureC, 1);
  } else {
    tft.print(F("--"));
  }
  tft.setTextSize(2);
  tft.print(F(" C"));

  tft.setTextSize(3);
  tft.setTextColor(kAccent);
  tft.setCursor(170, 48);
  if (!isnan(reading.humidityPct)) {
    tft.print(reading.humidityPct, 1);
  } else {
    tft.print(F("--"));
  }
  tft.setTextSize(2);
  tft.print(F(" %RH"));

  tft.setTextSize(2);
  tft.setTextColor(kText);
  tft.setCursor(8, 110);
  tft.print(F("eCO2: "));
  tft.setTextColor(kWarn);
  tft.print(reading.eco2Ppm);
  tft.setTextColor(kText);
  tft.print(F(" ppm"));

  tft.setCursor(8, 140);
  tft.print(F("TVOC: "));
  tft.print(reading.tvocPpb);
  tft.print(F(" ppb"));

  tft.setCursor(8, 170);
  tft.print(F("AQI: "));
  tft.setTextColor(aqiColor(reading.aqiUba));
  tft.print(reading.aqiUba);
  tft.setTextColor(kText);
  tft.print(F(" / 5"));

  tft.setCursor(8, 210);
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
    tft.setCursor(80, 120);
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
    const int x =
        kChartLeft + static_cast<int>((i * width) / (count > 1 ? count - 1 : 1));
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

  EnvChartBar bars[128];
  const size_t count = history.copyBars(bars, 128);

  tft.fillScreen(kBg);
  tft.setTextColor(kAccent);
  tft.setTextSize(2);
  tft.setCursor(8, 8);
  tft.print(F("Chart "));
  tft.print(history.stepLabel());
  tft.print(F("  "));
  tft.print(metricLabel(metric));

  drawChartBars_(bars, count, metric);
}
