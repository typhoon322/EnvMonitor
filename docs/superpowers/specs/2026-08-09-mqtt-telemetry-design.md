# EnvMonitor MQTT 遥测设计

**日期：** 2026-08-09  
**状态：** 已批准（用户确认方案 A）  
**范围：** 阳台设备经家里 Wi‑Fi 上报环境数据，供手机 / 电脑存档 / Home Assistant 共用

## 背景

当前固件仅有本地 TFT 与 USB 串口。用户希望将 ESP32-C3 模块放在阳台等位置，把温湿度与空气质量数据收集到室内端。阳台可连接家里 Wi‑Fi；需要同时支持浏览器/手机查看、电脑存档、Home Assistant，并保持可扩展。

## 目标

- 设备以 Wi‑Fi STA 接入家庭网络，经 **MQTT** 周期上报最新有效读数
- 同一 MQTT 流服务：Home Assistant、电脑订阅存档、手机（经 HA 或 MQTT 客户端）
- 可选局域网 **HTTP 状态页**，便于不装 HA 时临时查看
- 联网失败不得阻塞传感器采样与 TFT / CLI
- Wi‑Fi / MQTT 参数持久化到 NVS，支持串口 CLI 配置

## 非目标（本设计不包含）

- 云厂商专有协议绑定（阿里云 IoT / AWS IoT 等）
- 原生手机 App
- 设备端 SD / LittleFS 长期历史落盘
- 4G、ESP-NOW 中继（已确认阳台有 Wi‑Fi）
- OTA（可后续 Phase 单独做）

## 架构

```
EnvMonitor (ESP32-C3, 阳台)
    │  Wi‑Fi STA
    ▼
MQTT Broker（Mosquitto 等，家里常开设备或指定 broker）
    ├── Home Assistant（MQTT Discovery）
    ├── 电脑 / Node-RED / 脚本 → CSV / InfluxDB
    └── （可选）设备内置 HTTP GET /api/status
```

## 数据模型

### 上报 JSON（`state` 载荷）

字段名保持简短，便于嵌入式与脚本解析：

| 字段 | 类型 | 说明 |
|------|------|------|
| `t` | number | 温度 °C |
| `rh` | number | 相对湿度 % |
| `eco2` | number | eCO2 ppm |
| `tvoc` | number | TVOC ppb |
| `aqi` | number | UBA AQI 1–5；无效时可为 0 |
| `state` | string | 传感器状态文本（如 `OK` / `WARMUP` / `STARTUP` / `FAULT`） |
| `ts` | number | 设备 `millis()` 或可选 epoch（若已 NTP） |

示例：

```json
{"t":30.1,"rh":55.2,"eco2":450,"tvoc":40,"aqi":1,"state":"OK","ts":123456}
```

气体读数在 ENS160 无新样本时使用固件已有的「保持上次有效值」逻辑，避免上报全 0。

### MQTT 主题

| 主题 | 用途 |
|------|------|
| `envmonitor/<device_id>/state` | 周期状态（retain 建议开启，便于 HA/订阅端马上拿到最新） |
| `envmonitor/<device_id>/status` | 在线状态：`online` / `offline`（LWT = `offline`） |
| `homeassistant/sensor/<device_id>/+/config` | Phase 2：HA MQTT Discovery |

`device_id`：默认取芯片 MAC 后 6 位十六进制，或 CLI 可覆盖的短名（NVS）。

## 设备行为

### Wi‑Fi

- 模式：STA
- 凭据：NVS（SSID、密码）
- CLI：`wifi set <ssid> <pass>`、`wifi status`、`wifi clear`
- 连接策略：后台重试，指数退避上限（如 60 s），不阻塞 `loop()` 采样

### MQTT

- 参数 NVS：broker host、port（默认 1883）、user/pass（可选）、topic 前缀（默认 `envmonitor`）、`device_id`、上报间隔
- CLI：`mqtt set ...`、`mqtt status`、`mqtt interval <sec>`
- 上报周期：默认 **10 s**，可配置（建议范围 5–300 s）
- QoS：0 或 1（实现选 1 更稳妥于 retain 状态）
- LWT：`.../status` → `offline`；上线发布 `online`
- 断线：自动重连；未连接时跳过 publish，本地功能不受影响

### 与现有模块关系

- 新增模块建议：`src/net/wifi_manager.*`、`src/net/mqtt_telemetry.*`（或等价命名）
- `main.cpp` 在采样后调用 telemetry tick；不引入 FreeRTOS 任务（保持现有单线程 + WDT 约定）
- SystemContext / CLI 注入与现有 `SettingsStore` 扩展一致

### 可选 HTTP（Phase 3）

- 仅监听局域网，`GET /api/status` 返回与 MQTT 相同 JSON
- 可选极简 HTML 一页展示；不做复杂 Web UI / 配网门户（配网以 CLI 为主）

## 分阶段交付

### Phase 1 — 基础遥测（优先实现）

- Wi‑Fi STA + NVS + CLI
- MQTT 连接、LWT、周期 JSON 上报
- 文档：Mosquitto 最小安装示例、`mosquitto_sub` 验证命令

### Phase 2 — Home Assistant

- MQTT Discovery，暴露 temp / humidity / eco2 / tvoc / aqi 实体
- 文档：HA 接入步骤（MQTT 集成已启用的前提）

### Phase 3 — 局域网 HTTP（可选）

- `/api/status` + 可选静态页

### 并行文档（随 Phase 1）

- 电脑端订阅并追加 CSV 的示例脚本（bash 或 Python，短小即可）

## 安全与运维

- MQTT 用户密码存 NVS，串口 `mqtt status` **不回显密码**
- 默认不使用公网匿名 broker；文档可提「仅局域网 Mosquitto」
- TLS（8883）列为后续增强，Phase 1 不阻塞

## 成功标准

1. 设备连上家里 Wi‑Fi 后，10 s 内开始出现 MQTT `state` 消息  
2. 拔网线 / 关路由后，TFT 与本地采样仍正常；恢复网络后自动续传  
3. `mosquitto_sub` 能持续看到温湿度与气体字段变化  
4. Phase 2 完成后，HA 中自动出现对应传感器实体  

## 测试计划（摘要）

- 无 Wi‑Fi 凭据：本地功能正常，无崩溃  
- 错误密码：重试不阻塞，CLI 可见状态  
- Broker 可达：retain 的 state + status=online  
- 断 broker：LWT offline；重连后 online 与新 state  
- 上报间隔修改后立即按新间隔生效（下一拍）
