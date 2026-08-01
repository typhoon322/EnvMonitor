#include "cli/serial_cli.h"

#include "config.h"
#include "storage/settings_store.h"

void SerialCli::persistSettings_() {
  if (ctx_.settings != nullptr) {
    ctx_.settings->save(ctx_);
  }
}

void SerialCli::begin(unsigned long baud) {
  Serial.begin(baud);
  lineBuffer_.reserve(64);
  delay(100);
  Serial.println();
  Serial.println(F("Type 'help' for commands."));
}

void SerialCli::setContext(SystemContext context) {
  ctx_ = context;
}

void SerialCli::poll() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      if (lineBuffer_.length() > 0) {
        processLine(lineBuffer_);
        lineBuffer_ = "";
      }
      continue;
    }
    if (lineBuffer_.length() < 128) {
      lineBuffer_ += c;
    }
  }
}

bool SerialCli::parseChartStep_(const String &token, ChartStep &out) const {
  if (token.equalsIgnoreCase("3s")) {
    out = ChartStep::S3;
    return true;
  }
  if (token.equalsIgnoreCase("1m")) {
    out = ChartStep::M1;
    return true;
  }
  if (token.equalsIgnoreCase("5m")) {
    out = ChartStep::M5;
    return true;
  }
  if (token.equalsIgnoreCase("15m")) {
    out = ChartStep::M15;
    return true;
  }
  return false;
}

bool SerialCli::parseChartMetric_(const String &token, ChartMetric &out) const {
  if (token.equalsIgnoreCase("temp") || token.equalsIgnoreCase("temperature")) {
    out = ChartMetric::Temperature;
    return true;
  }
  if (token.equalsIgnoreCase("hum") || token.equalsIgnoreCase("humidity")) {
    out = ChartMetric::Humidity;
    return true;
  }
  if (token.equalsIgnoreCase("eco2") || token.equalsIgnoreCase("co2")) {
    out = ChartMetric::Eco2;
    return true;
  }
  return false;
}

void SerialCli::printHelp() const {
  Serial.println(F("Commands:"));
  Serial.println(F("  help                     Show this list"));
  Serial.println(F("  status                   Full sensor reading"));
  Serial.println(F("  view status|chart        Switch TFT view"));
  Serial.println(F("  chart [3s|1m|5m|15m]     Set or cycle chart step"));
  Serial.println(F("  metric temp|hum|eco2     Chart metric"));
  Serial.println(F("  save                     Save settings to NVS"));
  Serial.println(F("  load                     Reload settings from NVS"));
}

void SerialCli::printStatus() const {
  Serial.print(F("Board: "));
  Serial.println(F(BOARD_NAME));
  if (ctx_.sensor != nullptr) {
    Serial.print(F("Sensor: "));
    Serial.println(ctx_.sensor->stateText());
  }
  if (ctx_.reading != nullptr) {
    const AirQualityReading &r = *ctx_.reading;
    Serial.print(F("T="));
    if (!isnan(r.temperatureC)) {
      Serial.print(r.temperatureC, 2);
    } else {
      Serial.print(F("N/A"));
    }
    Serial.print(F("C RH="));
    if (!isnan(r.humidityPct)) {
      Serial.print(r.humidityPct, 1);
    } else {
      Serial.print(F("N/A"));
    }
    Serial.print(F("% eCO2="));
    Serial.print(r.eco2Ppm);
    Serial.print(F(" TVOC="));
    Serial.print(r.tvocPpb);
    Serial.print(F(" AQI="));
    Serial.println(r.aqiUba);
  }
  if (ctx_.displayView != nullptr) {
    Serial.print(F("View: "));
    Serial.println(*ctx_.displayView == DisplayView::Chart ? F("chart") : F("status"));
  }
  if (ctx_.chartStep != nullptr && ctx_.history != nullptr) {
    Serial.print(F("Chart step: "));
    Serial.println(ctx_.history->stepLabel());
  }
  if (ctx_.chartMetric != nullptr) {
    Serial.print(F("Chart metric: "));
    switch (*ctx_.chartMetric) {
      case ChartMetric::Humidity:
        Serial.println(F("humidity"));
        break;
      case ChartMetric::Eco2:
        Serial.println(F("eco2"));
        break;
      case ChartMetric::Temperature:
      default:
        Serial.println(F("temp"));
        break;
    }
  }
}

void SerialCli::processLine(const String &line) {
  String trimmed = line;
  trimmed.trim();
  if (trimmed.length() == 0) {
    return;
  }

  if (trimmed.equalsIgnoreCase("help")) {
    printHelp();
    return;
  }

  if (trimmed.equalsIgnoreCase("status")) {
    printStatus();
    return;
  }

  if (trimmed.equalsIgnoreCase("save")) {
    persistSettings_();
    Serial.println(F("Settings saved."));
    return;
  }

  if (trimmed.equalsIgnoreCase("load")) {
    if (ctx_.settings != nullptr) {
      if (ctx_.settings->load(ctx_)) {
        Serial.println(F("Settings loaded."));
      } else {
        Serial.println(F("No saved settings."));
      }
    }
    return;
  }

  if (trimmed.startsWith("view ")) {
    const String arg = trimmed.substring(5);
    if (arg.equalsIgnoreCase("status") && ctx_.displayView != nullptr) {
      *ctx_.displayView = DisplayView::Status;
      persistSettings_();
      Serial.println(F("View: status"));
      return;
    }
    if (arg.equalsIgnoreCase("chart") && ctx_.displayView != nullptr) {
      *ctx_.displayView = DisplayView::Chart;
      persistSettings_();
      Serial.println(F("View: chart"));
      return;
    }
    Serial.println(F("Usage: view status|chart"));
    return;
  }

  if (trimmed.startsWith("metric ")) {
    ChartMetric metric = ChartMetric::Temperature;
    if (parseChartMetric_(trimmed.substring(7), metric) && ctx_.chartMetric != nullptr) {
      *ctx_.chartMetric = metric;
      persistSettings_();
      Serial.println(F("Chart metric updated."));
      return;
    }
    Serial.println(F("Usage: metric temp|hum|eco2"));
    return;
  }

  if (trimmed.startsWith("chart")) {
    if (ctx_.history == nullptr || ctx_.chartStep == nullptr) {
      return;
    }
    String arg = trimmed.length() > 5 ? trimmed.substring(5) : String("");
    arg.trim();
    if (arg.length() == 0) {
      ctx_.history->cycleStep();
      *ctx_.chartStep = ctx_.history->step();
      persistSettings_();
      Serial.print(F("Chart step: "));
      Serial.println(ctx_.history->stepLabel());
      return;
    }
    ChartStep step = ChartStep::S3;
    if (parseChartStep_(arg, step)) {
      ctx_.history->setStep(step);
      *ctx_.chartStep = step;
      persistSettings_();
      Serial.print(F("Chart step: "));
      Serial.println(ctx_.history->stepLabel());
      return;
    }
    Serial.println(F("Usage: chart [3s|1m|5m|15m]"));
    return;
  }

  Serial.println(F("Unknown command. Type 'help'."));
}
