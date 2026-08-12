#include "display/display_driver.h"

#include <TFT_eSPI.h>
#include <math.h>
#include <time.h>

#include "config.h"
#include "deepseek/deepseek_whale_bmp.h"

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

  deepSeekChromeDrawn_ = false;
  resetDeepSeekCache_();

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
  resetDeepSeekCache_();

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
  resetDeepSeekCache_();

  tft.setTextFont(4);
  tft.setTextSize(1);
  tft.setTextColor(kAccent, kBg);
  tft.setCursor(6, 2);
  tft.print(F("DeepSeek"));
}

void DisplayDriver::resetDeepSeekCache_() {
  dsCacheValid_ = false;
  dsWifi_ = false;
  dsRefreshing_ = false;
  dsHasKey_ = false;
  dsValid_ = false;
  dsAvailable_ = false;
  dsLowBalance_ = false;
  dsKeyCount_ = 0;
  dsIntervalSec_ = 0;
  dsEpoch_ = 0;
  dsRemainSec_ = -2;
  dsWhaleX_ = -1;
  dsWhaleRight_ = true;
  dsWhaleSpout_ = false;
  dsSpouting_ = false;
  dsSpoutFrame_ = 255;
  dsSpoutUntilMs_ = 0;
  dsBalance_[0] = '\0';
  dsAnimating_ = false;
  dsAnimFrom_[0] = '\0';
  dsAnimTo_[0] = '\0';
}

void DisplayDriver::clearBalanceBand_() {
  // Wide fixed band so shorter numbers fully erase longer previous glyphs.
  clearField(4, 48, 312, 58);
}

void DisplayDriver::drawStaticBalance_(const char *balance, uint16_t color) {
  clearBalanceBand_();
  tft.setTextFont(kDsBalFont);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, kBg);
  tft.drawString(balance != nullptr ? balance : "", kDsBalCx, kDsBalCy);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
}

void DisplayDriver::startBalanceAnim_(const char *from, const char *to, uint16_t color) {
  strncpy(dsAnimFrom_, from != nullptr ? from : "", sizeof(dsAnimFrom_) - 1);
  dsAnimFrom_[sizeof(dsAnimFrom_) - 1] = '\0';
  strncpy(dsAnimTo_, to != nullptr ? to : "", sizeof(dsAnimTo_) - 1);
  dsAnimTo_[sizeof(dsAnimTo_) - 1] = '\0';
  dsAnimColor_ = color;
  dsAnimUp_ = atof(dsAnimTo_) >= atof(dsAnimFrom_);
  dsAnimStartMs_ = millis();
  dsAnimating_ = true;
}

void DisplayDriver::drawRollingBalance_(float t) {
  if (t < 0.0f) {
    t = 0.0f;
  }
  if (t > 1.0f) {
    t = 1.0f;
  }
  const float u = 1.0f - t;
  const float e = 1.0f - u * u * u;

  tft.setTextFont(kDsBalFont);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  const int cellH = tft.fontHeight();
  const int cellW = tft.textWidth("0") + 1;

  char fromPad[DEEPSEEK_BALANCE_LEN];
  char toPad[DEEPSEEK_BALANCE_LEN];
  const size_t fromLen = strlen(dsAnimFrom_);
  const size_t toLen = strlen(dsAnimTo_);
  const size_t width = fromLen > toLen ? fromLen : toLen;
  if (width == 0 || width >= DEEPSEEK_BALANCE_LEN) {
    drawStaticBalance_(dsAnimTo_, dsAnimColor_);
    return;
  }
  memset(fromPad, ' ', width);
  memset(toPad, ' ', width);
  fromPad[width] = '\0';
  toPad[width] = '\0';
  memcpy(fromPad + (width - fromLen), dsAnimFrom_, fromLen);
  memcpy(toPad + (width - toLen), dsAnimTo_, toLen);

  const int totalW = static_cast<int>(width) * cellW;
  int baseX = kDsBalCx - totalW / 2;
  if (baseX < 4) {
    baseX = 4;
  }
  const int baseY = kDsBalCy - cellH / 2;
  clearBalanceBand_();

  for (size_t i = 0; i < width; ++i) {
    const char a = fromPad[i];
    const char b = toPad[i];
    const int x = baseX + static_cast<int>(i) * cellW;
    if (a == b) {
      tft.setTextColor(dsAnimColor_, kBg);
      char s[2] = {a == ' ' ? '\0' : a, '\0'};
      if (s[0] != '\0') {
        tft.drawString(s, x, baseY);
      }
      continue;
    }

    tft.setViewport(x, baseY, cellW, cellH, false);
    tft.fillScreen(kBg);
    tft.setTextColor(dsAnimColor_, kBg);
    const int offset = static_cast<int>(e * cellH + 0.5f);
    char sa[2] = {a == ' ' ? '\0' : a, '\0'};
    char sb[2] = {b == ' ' ? '\0' : b, '\0'};
    if (dsAnimUp_) {
      if (sa[0] != '\0') {
        tft.drawString(sa, 0, -offset);
      }
      if (sb[0] != '\0') {
        tft.drawString(sb, 0, cellH - offset);
      }
    } else {
      if (sa[0] != '\0') {
        tft.drawString(sa, 0, offset);
      }
      if (sb[0] != '\0') {
        tft.drawString(sb, 0, offset - cellH);
      }
    }
    tft.resetViewport();
  }

  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
}

void DisplayDriver::drawDeepSeekMeta_(bool lowBalance, uint32_t lastRefreshEpoch, size_t count) {
  // Last successful refresh time under title (left).
  clearField(6, 26, 160, 20);
  tft.setTextFont(2);
  tft.setTextDatum(TL_DATUM);
  if (lastRefreshEpoch > 1700000000UL) {
    const time_t epoch = static_cast<time_t>(lastRefreshEpoch);
    struct tm tmInfo;
    localtime_r(&epoch, &tmInfo);
    char buf[20];
    snprintf(buf, sizeof(buf), "%02d-%02d %02d:%02d", tmInfo.tm_mon + 1, tmInfo.tm_mday,
             tmInfo.tm_hour, tmInfo.tm_min);
    tft.setTextColor(kText, kBg);
    tft.setCursor(6, 28);
    tft.print(buf);
  } else {
    tft.setTextColor(kChartGrid, kBg);
    tft.setCursor(6, 28);
    tft.print(F("--/-- --:--"));
  }

  // No currency line — only low-balance / multi-key hint under amount.
  clearField(40, 112, 240, 28);
  tft.setTextDatum(TC_DATUM);
  if (lowBalance) {
    tft.setTextFont(2);
    tft.setTextColor(kBad, kBg);
    tft.drawString("LOW BALANCE", kDsBalCx, 116);
  } else if (count > 1) {
    tft.setTextFont(1);
    tft.setTextColor(kText, kBg);
    char extra[12];
    snprintf(extra, sizeof(extra), "+%u keys", static_cast<unsigned>(count - 1));
    tft.drawString(extra, kDsBalCx, 118);
  }

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  tft.setTextSize(1);
}

void DisplayDriver::drawCountdownText_(int remainSec) {
  // Top-right under WiFi — keeps clear of the whale lane.
  clearField(200, 26, 116, 18);
  tft.setTextFont(2);
  tft.setTextDatum(TR_DATUM);
  uint16_t col = kAccent;
  char buf[8];
  if (remainSec < 0) {
    col = kChartGrid;
    snprintf(buf, sizeof(buf), "--:--");
  } else {
    if (remainSec <= 15) {
      col = kBad;
    } else if (remainSec <= 60) {
      col = kWarn;
    }
    const int m = remainSec / 60;
    const int s = remainSec % 60;
    snprintf(buf, sizeof(buf), "%d:%02d", m, s);
  }
  tft.setTextColor(col, kBg);
  tft.drawString(buf, 316, 28);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  tft.setTextSize(1);
}

float DisplayDriver::whalePathX_(float progress) {
  if (progress < 0.0f) {
    progress = 0.0f;
  }
  if (progress > 1.0f) {
    progress = 1.0f;
  }
  if (progress < 0.25f) {
    return 0.5f + 0.5f * (progress / 0.25f);
  }
  if (progress < 0.75f) {
    return 1.0f - (progress - 0.25f) / 0.5f;
  }
  return (progress - 0.75f) / 0.25f * 0.5f;
}

void DisplayDriver::drawSpoutPlume_(int bx, int by, uint8_t frame) {
  // Tall plume that can overlap balance — user OK with that during celebrate.
  const uint16_t c0 = color565(190, 230, 255);
  const uint16_t c1 = color565(120, 190, 255);
  const uint16_t c2 = color565(80, 150, 255);
  const int grow = 55 + static_cast<int>(frame) * 12;  // ~55..103 px tall
  const int top = by - grow;
  const int clippedTop = top < 18 ? 18 : top;

  // Stem
  tft.fillRect(bx - 1, clippedTop, 3, by - clippedTop, c1);
  tft.drawFastVLine(bx, clippedTop, by - clippedTop, c0);

  // Burst clouds near top
  const int burstY = clippedTop + 4;
  tft.fillCircle(bx, burstY, 5 + (frame % 3), c0);
  tft.fillCircle(bx - 8 - (frame % 2), burstY + 6, 4, c1);
  tft.fillCircle(bx + 8 + (frame % 2), burstY + 5, 4, c1);
  tft.fillCircle(bx - 14, burstY + 14, 3, c2);
  tft.fillCircle(bx + 14, burstY + 12, 3, c2);
  tft.fillCircle(bx, burstY + 16, 3, c1);

  // Droplets falling / spreading
  const int phase = static_cast<int>(frame);
  tft.fillCircle(bx - 6, burstY + 22 + phase, 2, c0);
  tft.fillCircle(bx + 7, burstY + 28 + (phase % 3), 2, c0);
  tft.fillCircle(bx - 11, burstY + 34, 2, c1);
  tft.fillCircle(bx + 12, burstY + 30, 2, c1);
  tft.drawPixel(bx - 18, burstY + 20 + phase, c0);
  tft.drawPixel(bx + 18, burstY + 18 + phase, c0);
  tft.drawPixel(bx - 4, clippedTop + 2, c0);
  tft.drawPixel(bx + 5, clippedTop + 3, c0);
}

void DisplayDriver::drawWhaleSprite_(int x, int y, bool facingRight, bool spout, uint8_t spoutFrame) {
  // Official DeepSeek whale mark (1-bit), brand blue #4D6BFE.
  const uint16_t blue = color565(0x4D, 0x6B, 0xFE);
  const uint8_t *bmp = facingRight ? kDsWhaleBmpRight : kDsWhaleBmpLeft;

  for (int row = 0; row < kDsWhaleBmpH; ++row) {
    const int rowBase = row * kDsWhaleBmpStride;
    for (int col = 0; col < kDsWhaleBmpW; ++col) {
      const uint8_t byte = pgm_read_byte(&bmp[rowBase + (col >> 3)]);
      if ((byte >> (7 - (col & 7))) & 0x01) {
        tft.drawPixel(x + col, y + row, blue);
      }
    }
  }

  if (spout) {
    const int bx = facingRight ? x + 30 : x + 22;
    const int by = y + 3;
    drawSpoutPlume_(bx, by, spoutFrame);
  }
}

void DisplayDriver::drawWhaleLane_(uint32_t lastRefreshMs, uint16_t intervalSec, bool celebrate) {
  float progress = 0.0f;
  if (intervalSec > 0 && lastRefreshMs > 0) {
    progress = static_cast<float>(millis() - lastRefreshMs) /
               (static_cast<float>(intervalSec) * 1000.0f);
    if (progress > 1.0f) {
      progress = 1.0f;
    }
  }

  // Celebrate only during explicit spout-hold / post-refresh — do NOT freeze early at 98%.
  if (celebrate && !dsSpouting_) {
    dsSpoutUntilMs_ = millis() + 2000UL;
  }
  if (!celebrate && dsSpouting_ && millis() > dsSpoutUntilMs_) {
    // end
  }

  const bool spout = celebrate || (millis() < dsSpoutUntilMs_);
  const uint8_t spoutFrame = static_cast<uint8_t>((millis() / 70UL) % 5UL);

  float frac = whalePathX_(progress);
  if (celebrate) {
    frac = 0.5f;  // center only while celebrating
  }

  const int minX = kDsLanePad;
  const int maxX = kScreenW - kDsWhaleW - kDsLanePad;
  const int x = minX + static_cast<int>(frac * static_cast<float>(maxX - minX) + 0.5f);

  bool facingRight = true;
  if (progress < 0.25f) {
    facingRight = true;
  } else if (progress < 0.75f) {
    facingRight = false;
  } else {
    facingRight = true;
  }
  if (celebrate) {
    facingRight = dsWhaleRight_;
  }

  dsSpouting_ = spout;
  if (dsWhaleX_ == x && dsWhaleRight_ == facingRight && dsWhaleSpout_ == spout &&
      (!spout || dsSpoutFrame_ == spoutFrame)) {
    return;
  }

  // Clear whale lane; when spouting also clear a tall plume column (may cover balance).
  clearField(0, kDsWhaleY - 4, kScreenW, kDsWhaleH + 8);
  if (spout) {
    const int bx = (facingRight ? x + 30 : x + 22);
    int left = bx - 28;
    if (left < 0) {
      left = 0;
    }
    int width = 56;
    if (left + width > kScreenW) {
      width = kScreenW - left;
    }
    clearField(left, 18, width, kDsWhaleY - 18);
  }
  drawWhaleSprite_(x, kDsWhaleY, facingRight, spout, spoutFrame);
  dsWhaleX_ = static_cast<int16_t>(x);
  dsWhaleRight_ = facingRight;
  dsWhaleSpout_ = spout;
  dsSpoutFrame_ = spoutFrame;
}

void DisplayDriver::updateDeepSeek(const DeepSeekBalanceEntry *entries, size_t count,
                                   bool wifiConnected, bool refreshing, bool spouting,
                                   uint32_t lastRefreshEpoch, uint32_t lastRefreshMs,
                                   uint16_t intervalSec) {
  if (!initialized_) {
    return;
  }

  if (!deepSeekChromeDrawn_) {
    drawDeepSeekChrome_();
  }

  const DeepSeekBalanceEntry *primary = nullptr;
  if (count > 0 && entries != nullptr) {
    primary = &entries[0];
    for (size_t i = 0; i < count; ++i) {
      if (entries[i].valid) {
        primary = &entries[i];
        break;
      }
    }
  }

  const bool hasKey = count > 0 && primary != nullptr;
  const bool valid = hasKey && primary->valid;
  const bool available = valid && primary->isAvailable;
  const char *balance = valid ? primary->totalBalance : "";
  const bool lowBalance = valid && (atof(balance) < DEEPSEEK_LOW_BALANCE);
  const uint16_t balColor = lowBalance ? kBad : (available ? kGood : kWarn);

  // Countdown always based on lastRefreshMs — keep ticking through refresh/fail.
  int remainSec = -1;
  if (intervalSec > 0) {
    if (lastRefreshMs == 0) {
      remainSec = static_cast<int>(intervalSec);
    } else {
      const uint32_t elapsedSec = (millis() - lastRefreshMs) / 1000UL;
      remainSec = elapsedSec >= intervalSec ? 0 : static_cast<int>(intervalSec - elapsedSec);
    }
  }
  const bool countdownChanged = !dsCacheValid_ || dsRemainSec_ != remainSec;

  if (dsAnimating_) {
    const uint32_t elapsed = millis() - dsAnimStartMs_;
    if (elapsed >= DS_BALANCE_ANIM_MS) {
      dsAnimating_ = false;
      drawStaticBalance_(dsAnimTo_, dsAnimColor_);
      strncpy(dsBalance_, dsAnimTo_, sizeof(dsBalance_) - 1);
      dsBalance_[sizeof(dsBalance_) - 1] = '\0';
    } else {
      drawRollingBalance_(static_cast<float>(elapsed) / static_cast<float>(DS_BALANCE_ANIM_MS));
    }
  }

  const bool wifiChanged = !dsCacheValid_ || dsWifi_ != wifiConnected;
  const bool balanceChanged =
      dsCacheValid_ && dsValid_ && valid && strncmp(dsBalance_, balance, sizeof(dsBalance_)) != 0;
  const bool metaChanged =
      !dsCacheValid_ || dsHasKey_ != hasKey || dsValid_ != valid || dsAvailable_ != available ||
      dsLowBalance_ != lowBalance || dsKeyCount_ != static_cast<uint8_t>(count) ||
      dsIntervalSec_ != intervalSec || dsEpoch_ != lastRefreshEpoch;
  const bool bodyChanged =
      balanceChanged || metaChanged || (!dsCacheValid_) || (dsValid_ != valid) || (dsHasKey_ != hasKey);

  if (wifiChanged) {
    clearField(220, 4, 96, 20);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(wifiConnected ? kGood : kBad, kBg);
    tft.setCursor(240, 8);
    tft.print(wifiConnected ? F("WiFi OK") : F("No WiFi"));
    dsWifi_ = wifiConnected;
  }

  if (hasKey) {
    if (countdownChanged || !dsCacheValid_) {
      drawCountdownText_(remainSec);
      dsRemainSec_ = static_cast<int16_t>(remainSec);
    }
    const bool celebrate = spouting || refreshing;
    const bool wasCelebrating = dsSpouting_;
    drawWhaleLane_(lastRefreshMs, intervalSec, celebrate);
    // Plume may have covered the balance — restore when celebration ends.
    if (wasCelebrating && !dsSpouting_ && valid) {
      drawStaticBalance_(balance, balColor);
      drawDeepSeekMeta_(lowBalance, lastRefreshEpoch, count);
      drawCountdownText_(remainSec);
    }
  }

  if (dsAnimating_ && !metaChanged && !wifiChanged && !balanceChanged) {
    tft.setTextFont(1);
    tft.setTextSize(1);
    dsRefreshing_ = refreshing;
    return;
  }

  if (!bodyChanged && !dsAnimating_) {
    tft.setTextFont(1);
    tft.setTextSize(1);
    dsRefreshing_ = refreshing;
    return;
  }

  // First paint / structural change.
  if (!dsCacheValid_ || dsHasKey_ != hasKey || dsValid_ != valid || (!valid && bodyChanged)) {
    dsAnimating_ = false;
    clearField(8, 48, 280, 88);

    if (!hasKey) {
      tft.setTextFont(2);
      tft.setTextSize(1);
      tft.setTextColor(kWarn, kBg);
      tft.setCursor(8, 48);
      tft.print(F("No API Key"));
      tft.setTextFont(1);
      tft.setTextColor(kText, kBg);
      tft.setCursor(8, 78);
      tft.print(F("Configure via WebUI or:"));
      tft.setCursor(8, 92);
      tft.print(F("ds add <name> <key>"));
      tft.setCursor(8, 110);
      tft.print(F("AP: EnvMonitor"));
      tft.setCursor(8, 124);
      tft.print(F("open http://192.168.4.1"));
    } else if (!valid) {
      tft.setTextFont(2);
      tft.setTextSize(1);
      tft.setTextColor(kText, kBg);
      tft.setCursor(8, 70);
      tft.print(wifiConnected ? F("Waiting for balance") : F("Connect WiFi to fetch"));
      drawCountdownText_(remainSec);
      drawWhaleLane_(lastRefreshMs, intervalSec, spouting || refreshing);
    } else {
      drawStaticBalance_(balance, balColor);
      drawDeepSeekMeta_(lowBalance, lastRefreshEpoch, count);
      drawCountdownText_(remainSec);
      drawWhaleLane_(lastRefreshMs, intervalSec, spouting || refreshing);
    }
  } else if (balanceChanged && valid) {
    startBalanceAnim_(dsBalance_, balance, balColor);
    drawRollingBalance_(0.0f);
    if (metaChanged) {
      drawDeepSeekMeta_(lowBalance, lastRefreshEpoch, count);
    }
  } else if (metaChanged && valid) {
    drawDeepSeekMeta_(lowBalance, lastRefreshEpoch, count);
  }

  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);

  dsCacheValid_ = true;
  dsHasKey_ = hasKey;
  dsValid_ = valid;
  dsAvailable_ = available;
  dsLowBalance_ = lowBalance;
  dsRefreshing_ = refreshing;
  dsKeyCount_ = static_cast<uint8_t>(count);
  dsIntervalSec_ = intervalSec;
  dsEpoch_ = lastRefreshEpoch;
  dsRemainSec_ = static_cast<int16_t>(remainSec);
  strncpy(dsBalance_, balance, sizeof(dsBalance_) - 1);
  dsBalance_[sizeof(dsBalance_) - 1] = '\0';
}
