#include "storage/settings_store.h"

#include <Preferences.h>

#include "config.h"

namespace {
Preferences prefs;
}  // namespace

bool SettingsStore::begin() {
  opened_ = prefs.begin(kNamespace, false);
  return opened_;
}

bool SettingsStore::load(SystemContext &ctx) {
  if (!opened_) {
    return false;
  }

  const uint32_t magic = prefs.getUInt("magic", 0);
  if (magic != kMagicV1) {
    return false;
  }

  if (ctx.displayView != nullptr) {
    const uint8_t view = prefs.getUChar("view", static_cast<uint8_t>(DisplayView::Status));
    *ctx.displayView = view == static_cast<uint8_t>(DisplayView::Chart) ? DisplayView::Chart
                                                                        : DisplayView::Status;
  }

  if (ctx.chartMetric != nullptr) {
    const uint8_t metric = prefs.getUChar("metric", static_cast<uint8_t>(ChartMetric::Temperature));
    if (metric < static_cast<uint8_t>(ChartMetric::Count)) {
      *ctx.chartMetric = static_cast<ChartMetric>(metric);
    }
  }

  if (ctx.chartStep != nullptr && ctx.history != nullptr) {
    const uint8_t step = prefs.getUChar("cStep", static_cast<uint8_t>(ChartStep::S3));
    if (step < static_cast<uint8_t>(ChartStep::Count)) {
      const ChartStep chartStep = static_cast<ChartStep>(step);
      ctx.history->setStep(chartStep);
      *ctx.chartStep = chartStep;
    }
  }

  if (ctx.backlightLevel != nullptr) {
    *ctx.backlightLevel = prefs.getUChar("bl", DEFAULT_BACKLIGHT_LEVEL);
  }

  return true;
}

bool SettingsStore::save(const SystemContext &ctx) {
  if (!opened_) {
    return false;
  }

  prefs.putUInt("magic", kMagicV1);

  if (ctx.displayView != nullptr) {
    prefs.putUChar("view", static_cast<uint8_t>(*ctx.displayView));
  }

  if (ctx.chartMetric != nullptr) {
    prefs.putUChar("metric", static_cast<uint8_t>(*ctx.chartMetric));
  }

  if (ctx.chartStep != nullptr) {
    prefs.putUChar("cStep", static_cast<uint8_t>(*ctx.chartStep));
  }

  if (ctx.backlightLevel != nullptr) {
    prefs.putUChar("bl", *ctx.backlightLevel);
  }

  return true;
}
