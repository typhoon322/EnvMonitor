#include "history/env_history.h"

#include "config.h"

namespace {
void absorbBar(EnvChartBar &dst, const EnvChartBar &src, bool starting) {
  if (starting) {
    dst = src;
    return;
  }

  if (src.tHigh > dst.tHigh) {
    dst.tHigh = src.tHigh;
  }
  if (src.tLow < dst.tLow) {
    dst.tLow = src.tLow;
  }
  dst.tClose = src.tClose;

  if (src.hHigh > dst.hHigh) {
    dst.hHigh = src.hHigh;
  }
  if (src.hLow < dst.hLow) {
    dst.hLow = src.hLow;
  }
  dst.hClose = src.hClose;

  if (src.eHigh > dst.eHigh) {
    dst.eHigh = src.eHigh;
  }
  if (src.eLow < dst.eLow) {
    dst.eLow = src.eLow;
  }
  dst.eClose = src.eClose;
}
}  // namespace

void EnvHistory::begin() {
  ring3s_ = {bars3s_, kCap3s, 0, 0};
  ring1m_ = {bars1m_, kCap1m, 0, 0};
  ring5m_ = {bars5m_, kCap5m, 0, 0};
  ring15m_ = {bars15m_, kCap15m, 0, 0};
  step_ = ChartStep::S3;
  hasOpen1m_ = hasOpen5m_ = hasOpen15m_ = false;
  countIn1m_ = countIn5m_ = countIn15m_ = 0;
}

void EnvHistory::ringPush(Ring &ring, const EnvChartBar &bar) {
  ring.data[ring.head] = bar;
  ring.head = (ring.head + 1) % ring.capacity;
  if (ring.count < ring.capacity) {
    ring.count++;
  }
}

void EnvHistory::mergeMetric_(float value, float &high, float &low, float &close, bool starting) {
  if (starting) {
    high = value;
    low = value;
    close = value;
    return;
  }
  if (value > high) {
    high = value;
  }
  if (value < low) {
    low = value;
  }
  close = value;
}

void EnvHistory::mergeBar_(EnvChartBar &dst, float tempC, float humPct, float eco2,
                           bool starting) {
  mergeMetric_(tempC, dst.tHigh, dst.tLow, dst.tClose, starting);
  mergeMetric_(humPct, dst.hHigh, dst.hLow, dst.hClose, starting);
  mergeMetric_(eco2, dst.eHigh, dst.eLow, dst.eClose, starting);
}

void EnvHistory::absorbBar_(EnvChartBar &dst, const EnvChartBar &src, bool starting) {
  absorbBar(dst, src, starting);
}

void EnvHistory::push(float tempC, float humPct, float eco2Ppm) {
  if (isnan(tempC) || isnan(humPct) || isnan(eco2Ppm)) {
    return;
  }

  EnvChartBar bar3{};
  mergeBar_(bar3, tempC, humPct, eco2Ppm, true);
  ringPush(ring3s_, bar3);

  mergeBar_(open1m_, tempC, humPct, eco2Ppm, !hasOpen1m_);
  hasOpen1m_ = true;
  countIn1m_++;
  if (countIn1m_ < kRoll3sTo1m) {
    return;
  }

  ringPush(ring1m_, open1m_);
  absorbBar_(open5m_, open1m_, !hasOpen5m_);
  hasOpen5m_ = true;
  countIn5m_++;
  hasOpen1m_ = false;
  countIn1m_ = 0;

  if (countIn5m_ < kRoll1mTo5m) {
    return;
  }

  ringPush(ring5m_, open5m_);
  absorbBar_(open15m_, open5m_, !hasOpen15m_);
  hasOpen15m_ = true;
  countIn15m_++;
  hasOpen5m_ = false;
  countIn5m_ = 0;

  if (countIn15m_ < kRoll5mTo15m) {
    return;
  }

  ringPush(ring15m_, open15m_);
  hasOpen15m_ = false;
  countIn15m_ = 0;
}

void EnvHistory::setStep(ChartStep step) {
  if (step < ChartStep::Count) {
    step_ = step;
  }
}

void EnvHistory::cycleStep() {
  const uint8_t next =
      (static_cast<uint8_t>(step_) + 1) % static_cast<uint8_t>(ChartStep::Count);
  step_ = static_cast<ChartStep>(next);
}

size_t EnvHistory::visibleBarLimit(ChartStep step) {
  switch (step) {
    case ChartStep::S3:
      return kVis3s;
    case ChartStep::M1:
      return kVis1m;
    case ChartStep::M5:
      return kVis5m;
    case ChartStep::M15:
      return kVis15m;
    default:
      return kVis3s;
  }
}

const char *EnvHistory::stepLabel() const {
  switch (step_) {
    case ChartStep::S3:
      return "3s";
    case ChartStep::M1:
      return "1m";
    case ChartStep::M5:
      return "5m";
    case ChartStep::M15:
      return "15m";
    default:
      return "?";
  }
}

const EnvHistory::Ring &EnvHistory::activeRing_() const {
  switch (step_) {
    case ChartStep::M1:
      return ring1m_;
    case ChartStep::M5:
      return ring5m_;
    case ChartStep::M15:
      return ring15m_;
    case ChartStep::S3:
    default:
      return ring3s_;
  }
}

size_t EnvHistory::barCount() const {
  const Ring &ring = activeRing_();
  const size_t limit = visibleBarLimit(step_);
  return ring.count < limit ? ring.count : limit;
}

size_t EnvHistory::copyBars(EnvChartBar *out, size_t maxCount) const {
  if (out == nullptr || maxCount == 0) {
    return 0;
  }
  const Ring &ring = activeRing_();
  const size_t limit = visibleBarLimit(step_);
  size_t n = ring.count;
  if (n > limit) {
    n = limit;
  }
  if (n > maxCount) {
    n = maxCount;
  }
  if (ring.count > 0 && n > 0) {
    const size_t start = (ring.head + ring.capacity - n) % ring.capacity;
    for (size_t i = 0; i < n; ++i) {
      out[i] = ring.data[(start + i) % ring.capacity];
    }
  } else {
    n = 0;
  }

  if (n < maxCount && n < limit) {
    const EnvChartBar *open = nullptr;
    bool has = false;
    switch (step_) {
      case ChartStep::M1:
        open = &open1m_;
        has = hasOpen1m_;
        break;
      case ChartStep::M5:
        open = &open5m_;
        has = hasOpen5m_;
        break;
      case ChartStep::M15:
        open = &open15m_;
        has = hasOpen15m_;
        break;
      default:
        break;
    }
    if (has && open != nullptr) {
      out[n] = *open;
      return n + 1;
    }
  }
  return n;
}

void EnvHistory::barValues(const EnvChartBar &bar, ChartMetric metric, float &high, float &low,
                           float &close) {
  switch (metric) {
    case ChartMetric::Humidity:
      high = bar.hHigh;
      low = bar.hLow;
      close = bar.hClose;
      break;
    case ChartMetric::Eco2:
      high = bar.eHigh;
      low = bar.eLow;
      close = bar.eClose;
      break;
    case ChartMetric::Temperature:
    default:
      high = bar.tHigh;
      low = bar.tLow;
      close = bar.tClose;
      break;
  }
}
