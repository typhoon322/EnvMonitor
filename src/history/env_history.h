#pragma once

#include <Arduino.h>
#include <math.h>

struct EnvChartBar {
  float tHigh;
  float tLow;
  float tClose;
  float hHigh;
  float hLow;
  float hClose;
  float eHigh;
  float eLow;
  float eClose;
};

enum class ChartStep : uint8_t {
  S3 = 0,
  M1,
  M5,
  M15,
  Count
};

enum class DisplayView : uint8_t { Status = 0, Chart, DeepSeek };

enum class ChartMetric : uint8_t { Temperature = 0, Humidity, Eco2, Count };

class EnvHistory {
public:
  void begin();
  void push(float tempC, float humPct, float eco2Ppm);

  ChartStep step() const { return step_; }
  void setStep(ChartStep step);
  void cycleStep();
  const char *stepLabel() const;
  static size_t visibleBarLimit(ChartStep step);

  size_t copyBars(EnvChartBar *out, size_t maxCount) const;
  size_t barCount() const;

  static void barValues(const EnvChartBar &bar, ChartMetric metric, float &high, float &low,
                        float &close);

private:
  static constexpr size_t kCap3s = 128;
  static constexpr size_t kCap1m = 720;
  static constexpr size_t kCap5m = 72;
  static constexpr size_t kCap15m = 48;

  static constexpr size_t kVis3s = 128;
  static constexpr size_t kVis1m = 60;
  static constexpr size_t kVis5m = 72;
  static constexpr size_t kVis15m = 48;

  static constexpr size_t kRoll3sTo1m = 20;
  static constexpr size_t kRoll1mTo5m = 5;
  static constexpr size_t kRoll5mTo15m = 3;

  struct Ring {
    EnvChartBar *data;
    size_t capacity;
    size_t head;
    size_t count;
  };

  EnvChartBar bars3s_[kCap3s] = {};
  EnvChartBar bars1m_[kCap1m] = {};
  EnvChartBar bars5m_[kCap5m] = {};
  EnvChartBar bars15m_[kCap15m] = {};

  Ring ring3s_{};
  Ring ring1m_{};
  Ring ring5m_{};
  Ring ring15m_{};

  EnvChartBar open1m_{};
  EnvChartBar open5m_{};
  EnvChartBar open15m_{};
  bool hasOpen1m_ = false;
  bool hasOpen5m_ = false;
  bool hasOpen15m_ = false;
  size_t countIn1m_ = 0;
  size_t countIn5m_ = 0;
  size_t countIn15m_ = 0;

  ChartStep step_ = ChartStep::S3;

  static void ringPush(Ring &ring, const EnvChartBar &bar);
  static void mergeMetric_(float value, float &high, float &low, float &close, bool starting);
  static void mergeBar_(EnvChartBar &dst, float tempC, float humPct, float eco2, bool starting);
  static void absorbBar_(EnvChartBar &dst, const EnvChartBar &src, bool starting);
  const Ring &activeRing_() const;
};
