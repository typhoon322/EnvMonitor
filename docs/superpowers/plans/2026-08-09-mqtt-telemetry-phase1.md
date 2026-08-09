# MQTT Telemetry Phase 1 Implementation Plan

> **For agentic workers:** Execute task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** EnvMonitor connects to home Wi‑Fi and periodically publishes sensor JSON to MQTT without blocking local sampling/TFT/CLI.

**Architecture:** Non-blocking `WifiManager` + `MqttTelemetry` ticked from `loop()`; credentials and MQTT config in NVS via `SettingsStore`; serial CLI for configuration.

**Tech Stack:** Arduino-ESP32 `WiFi.h`, `knolleary/PubSubClient`, `bblanchon/ArduinoJson@^7`, existing Preferences/NVS.

**Scope:** Phase 1 only (no HA Discovery, no HTTP). Spec: `docs/superpowers/specs/2026-08-09-mqtt-telemetry-design.md`.

---

## File map

| File | Role |
|------|------|
| `src/net/wifi_manager.h/.cpp` | STA connect / reconnect tick |
| `src/net/mqtt_telemetry.h/.cpp` | MQTT client, LWT, JSON publish |
| `src/storage/settings_store.*` | WiFi + MQTT NVS load/save |
| `src/cli/serial_cli.*` | `wifi` / `mqtt` commands + SystemContext pointers |
| `src/main.cpp` | begin + tick wiring |
| `platformio.ini` | PubSubClient + ArduinoJson |
| `include/config.h` | defaults (interval, topic prefix) |
| `docs/protocol.md` | CLI docs |
| `docs/mqtt-telemetry.md` | Mosquitto + mosquitto_sub + CSV example |

---

### Task 1: Dependencies and config defaults

**Files:**
- Modify: `platformio.ini`
- Modify: `include/config.h`

- [ ] Add to `lib_deps`: `knolleary/PubSubClient@^2.8`, `bblanchon/ArduinoJson@^7.2.1`
- [ ] Add defaults in `config.h`:

```cpp
#define MQTT_DEFAULT_PORT 1883
#define MQTT_DEFAULT_PREFIX "envmonitor"
#define MQTT_DEFAULT_INTERVAL_SEC 10
#define MQTT_INTERVAL_MIN_SEC 5
#define MQTT_INTERVAL_MAX_SEC 300
#define WIFI_RECONNECT_MIN_MS 5000
#define WIFI_RECONNECT_MAX_MS 60000
```

- [ ] Build to confirm deps resolve: `pio run -e esp32-c3-envmonitor`

---

### Task 2: SettingsStore WiFi + MQTT APIs

**Files:**
- Modify: `src/storage/settings_store.h`
- Modify: `src/storage/settings_store.cpp`

- [ ] Add public methods (do not require magic bump; missing keys → empty/defaults):

```cpp
void loadWifiCredentials(char *ssid, size_t ssidLen, char *pass, size_t passLen) const;
bool saveWifiCredentials(const char *ssid, const char *pass);
void clearWifiCredentials();

struct MqttConfig {
  char host[64] = "";
  uint16_t port = MQTT_DEFAULT_PORT;
  char user[32] = "";
  char pass[64] = "";
  char prefix[32] = MQTT_DEFAULT_PREFIX;
  char deviceId[24] = "";  // empty => derive from MAC at runtime
  uint16_t intervalSec = MQTT_DEFAULT_INTERVAL_SEC;
};

void loadMqttConfig(MqttConfig &cfg) const;
bool saveMqttConfig(const MqttConfig &cfg);
```

- [ ] NVS keys: `wifiSsid`, `wifiPass`, `mqttHost`, `mqttPort`, `mqttUser`, `mqttPass`, `mqttPrefix`, `deviceId`, `mqttIntv`

---

### Task 3: WifiManager

**Files:**
- Create: `src/net/wifi_manager.h`
- Create: `src/net/wifi_manager.cpp`

API:

```cpp
class WifiManager {
public:
  void begin(SettingsStore *settings);
  void tick(uint32_t nowMs);
  bool isConnected() const;
  bool setCredentials(const char *ssid, const char *pass); // save + reconnect
  void clearCredentials();
  void printStatus() const;
  IPAddress localIP() const;
private:
  void tryConnect_();
  SettingsStore *settings_ = nullptr;
  bool hasCreds_ = false;
  uint32_t nextRetryMs_ = 0;
  uint32_t backoffMs_ = WIFI_RECONNECT_MIN_MS;
};
```

Behavior: STA only; if no SSID, stay idle; exponential backoff reconnect when disconnected; never block > few ms.

---

### Task 4: MqttTelemetry

**Files:**
- Create: `src/net/mqtt_telemetry.h`
- Create: `src/net/mqtt_telemetry.cpp`

API:

```cpp
class MqttTelemetry {
public:
  void begin(SettingsStore *settings, WifiManager *wifi);
  void tick(uint32_t nowMs, const AirQualityReading &reading, const char *sensorState);
  void setConfig(const SettingsStore::MqttConfig &cfg); // apply + save via settings
  void printStatus() const;
  uint16_t intervalSec() const;
  bool setIntervalSec(uint16_t sec);
private:
  void ensureClient_();
  void publishState_(...);
  char deviceIdResolved_[24];
  // PubSubClient, WiFiClient, topics, timers
};
```

Topics: `<prefix>/<device_id>/state` (retain, QoS1), `<prefix>/<device_id>/status` LWT=`offline`, on connect publish `online`.

JSON: `{"t":..,"rh":..,"eco2":..,"tvoc":..,"aqi":..,"state":"..","ts":millis}`. Skip NaN temp/rh as `null`.

---

### Task 5: CLI + main wiring

**Files:**
- Modify: `src/cli/serial_cli.h` (add `WifiManager*`, `MqttTelemetry*` to SystemContext)
- Modify: `src/cli/serial_cli.cpp` (commands + help)
- Modify: `src/main.cpp`

CLI:
- `wifi set <ssid> <pass>` (pass may be empty for open networks: `wifi set MySSID ""`)
- `wifi status` / `wifi clear`
- `mqtt set <host> [port] [user] [pass]`
- `mqtt prefix <prefix>` / `mqtt id <device_id>` / `mqtt interval <sec>`
- `mqtt status`

- [ ] `setup()`: after settings load, `wifiManager.begin`, `mqttTelemetry.begin`
- [ ] `loop()`: `wifiManager.tick(now)`; `mqttTelemetry.tick(now, currentReading, airQualitySensor.stateText())`

---

### Task 6: Docs + verify build

**Files:**
- Create: `docs/mqtt-telemetry.md`
- Modify: `docs/protocol.md`
- Modify: `docs/features-and-notes.md` (mark WiFi/MQTT Phase 1 done)

- [ ] `pio run -e esp32-c3-envmonitor` SUCCESS
- [ ] Upload if USB present

---

## Spec coverage (Phase 1)

| Spec item | Task |
|-----------|------|
| Wi‑Fi STA + NVS + CLI | 2, 3, 5 |
| MQTT LWT + JSON publish | 4, 5 |
| Non-blocking | 3, 4 |
| Mosquitto / sub / CSV docs | 6 |
| HA Discovery / HTTP | deferred Phase 2/3 |
