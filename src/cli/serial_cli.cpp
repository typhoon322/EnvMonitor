#include "cli/serial_cli.h"

#include "config.h"
#include "net/mqtt_telemetry.h"
#include "net/wifi_manager.h"
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
  Serial.println(F("  wifi set <ssid> [pass]   Save Wi-Fi and connect"));
  Serial.println(F("  wifi status|clear        Wi-Fi status / clear creds"));
  Serial.println(F("  mqtt set <host> [port] [user] [pass]"));
  Serial.println(F("  mqtt prefix|id|interval  MQTT config"));
  Serial.println(F("  mqtt status              MQTT status (pass hidden)"));
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
  if (ctx_.wifi != nullptr) {
    ctx_.wifi->printStatus();
  }
  if (ctx_.mqtt != nullptr) {
    ctx_.mqtt->printStatus();
  }
}

void SerialCli::handleWifi_(const String &args) {
  if (ctx_.wifi == nullptr) {
    Serial.println(F("WiFi unavailable."));
    return;
  }
  String rest = args;
  rest.trim();
  if (rest.equalsIgnoreCase("status") || rest.length() == 0) {
    ctx_.wifi->printStatus();
    return;
  }
  if (rest.equalsIgnoreCase("clear")) {
    ctx_.wifi->clearCredentials();
    return;
  }
  if (rest.startsWith("set ")) {
    String payload = rest.substring(4);
    payload.trim();
    if (payload.length() == 0) {
      Serial.println(F("Usage: wifi set <ssid> [pass]"));
      return;
    }
    const int sp = payload.indexOf(' ');
    String ssid;
    String pass;
    if (sp < 0) {
      ssid = payload;
    } else {
      ssid = payload.substring(0, sp);
      pass = payload.substring(sp + 1);
      pass.trim();
      if (pass == "\"\"") {
        pass = "";
      }
    }
    if (ctx_.wifi->setCredentials(ssid.c_str(), pass.c_str())) {
      Serial.println(F("WiFi credentials saved; connecting..."));
    } else {
      Serial.println(F("WiFi set failed."));
    }
    return;
  }
  Serial.println(F("Usage: wifi set <ssid> [pass] | wifi status | wifi clear"));
}

void SerialCli::handleMqtt_(const String &args) {
  if (ctx_.mqtt == nullptr || ctx_.settings == nullptr) {
    Serial.println(F("MQTT unavailable."));
    return;
  }
  String rest = args;
  rest.trim();
  if (rest.equalsIgnoreCase("status") || rest.length() == 0) {
    ctx_.mqtt->printStatus();
    return;
  }
  if (rest.startsWith("interval ")) {
    const int sec = rest.substring(9).toInt();
    if (ctx_.mqtt->setIntervalSec(static_cast<uint16_t>(sec))) {
      Serial.print(F("MQTT interval: "));
      Serial.print(sec);
      Serial.println(F("s"));
    } else {
      Serial.println(F("Usage: mqtt interval <5-300>"));
    }
    return;
  }
  if (rest.startsWith("prefix ")) {
    MqttConfig cfg;
    ctx_.settings->loadMqttConfig(cfg);
    String prefix = rest.substring(7);
    prefix.trim();
    if (prefix.length() == 0 || prefix.length() >= static_cast<int>(sizeof(cfg.prefix))) {
      Serial.println(F("Usage: mqtt prefix <prefix>"));
      return;
    }
    strncpy(cfg.prefix, prefix.c_str(), sizeof(cfg.prefix) - 1);
    cfg.prefix[sizeof(cfg.prefix) - 1] = '\0';
    ctx_.mqtt->applyConfig(cfg);
    Serial.println(F("MQTT prefix saved."));
    return;
  }
  if (rest.startsWith("id ")) {
    MqttConfig cfg;
    ctx_.settings->loadMqttConfig(cfg);
    String id = rest.substring(3);
    id.trim();
    if (id.length() >= static_cast<int>(sizeof(cfg.deviceId))) {
      Serial.println(F("device id too long"));
      return;
    }
    strncpy(cfg.deviceId, id.c_str(), sizeof(cfg.deviceId) - 1);
    cfg.deviceId[sizeof(cfg.deviceId) - 1] = '\0';
    ctx_.mqtt->applyConfig(cfg);
    Serial.println(F("MQTT device id saved."));
    return;
  }
  if (rest.startsWith("set ")) {
    String payload = rest.substring(4);
    payload.trim();
    if (payload.length() == 0) {
      Serial.println(F("Usage: mqtt set <host> [port] [user] [pass]"));
      return;
    }
    MqttConfig cfg;
    ctx_.settings->loadMqttConfig(cfg);

    String token;
    int idx = 0;
    auto nextToken = [&](String &out) -> bool {
      while (idx < static_cast<int>(payload.length()) && payload[idx] == ' ') {
        ++idx;
      }
      if (idx >= static_cast<int>(payload.length())) {
        return false;
      }
      const int start = idx;
      while (idx < static_cast<int>(payload.length()) && payload[idx] != ' ') {
        ++idx;
      }
      out = payload.substring(start, idx);
      return true;
    };

    if (!nextToken(token)) {
      Serial.println(F("Usage: mqtt set <host> [port] [user] [pass]"));
      return;
    }
    strncpy(cfg.host, token.c_str(), sizeof(cfg.host) - 1);
    cfg.host[sizeof(cfg.host) - 1] = '\0';

    if (nextToken(token)) {
      const int port = token.toInt();
      if (port > 0 && port <= 65535) {
        cfg.port = static_cast<uint16_t>(port);
      } else {
        // token was user, not port
        strncpy(cfg.user, token.c_str(), sizeof(cfg.user) - 1);
        cfg.user[sizeof(cfg.user) - 1] = '\0';
        if (nextToken(token)) {
          strncpy(cfg.pass, token.c_str(), sizeof(cfg.pass) - 1);
          cfg.pass[sizeof(cfg.pass) - 1] = '\0';
        }
        ctx_.mqtt->applyConfig(cfg);
        Serial.println(F("MQTT config saved."));
        return;
      }
    }
    if (nextToken(token)) {
      strncpy(cfg.user, token.c_str(), sizeof(cfg.user) - 1);
      cfg.user[sizeof(cfg.user) - 1] = '\0';
    }
    if (nextToken(token)) {
      strncpy(cfg.pass, token.c_str(), sizeof(cfg.pass) - 1);
      cfg.pass[sizeof(cfg.pass) - 1] = '\0';
    }
    ctx_.mqtt->applyConfig(cfg);
    Serial.println(F("MQTT config saved."));
    return;
  }
  Serial.println(F("Usage: mqtt set|status|prefix|id|interval"));
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

  if (trimmed.startsWith("wifi")) {
    String args = trimmed.length() > 4 ? trimmed.substring(4) : String("");
    args.trim();
    handleWifi_(args);
    return;
  }

  if (trimmed.startsWith("mqtt")) {
    String args = trimmed.length() > 4 ? trimmed.substring(4) : String("");
    args.trim();
    handleMqtt_(args);
    return;
  }

  Serial.println(F("Unknown command. Type 'help'."));
}
