# SoftAP + WebUI + DeepSeek 拉取规则设计

日期：2026-08-11  
状态：已批准实施实现

## 目标

在 ESP32-C3 EnvMonitor 上提供：

1. **未连上路由器时开 SoftAP**，连上后关 AP  
2. **板载 WebUI**（连 AP 的 `192.168.4.1` 或内网 STA IP）配置 WiFi / MQTT / DeepSeek / TFT 视图，并展示环境监测数据  
3. **DeepSeek 余额**：仅 DS 视图自动请求；3 分钟轮询；余额 NVS 持久化；无 Key 时屏幕提示  

不设 Web 登录密码。串口 CLI 保留。

## WiFi / AP

| 状态 | 模式 |
|------|------|
| 任意状态 | SoftAP **始终开启**（`WIFI_AP_STA`） |
| STA 已连接 | 可用 AP 或内网 IP 访问 WebUI |

- AP SSID：固定 `EnvMonitor`
- AP 密码：`envmonitor`
- AP IP：`192.168.4.1`  

## WebUI（方案 A）

- Arduino `WebServer` 端口 80，单线程 `handleClient()`  
- 单页 HTML（PROGMEM）：状态区 + 设置区  
- 状态约 2s 轮询 `GET /api/status`  
- 无鉴权  

### API

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/` | HTML |
| GET | `/api/status` | 环境数据、WiFi/AP、DeepSeek 摘要 |
| GET | `/api/config` | 配置（密码/Key 脱敏） |
| POST | `/api/wifi` | `{ssid,pass}` |
| POST | `/api/mqtt` | host/port/user/pass/prefix/id/interval |
| POST | `/api/deepseek` | action: add/del/interval/refresh |
| POST | `/api/view` | `{view:status\|chart\|ds}` |

## DeepSeek

| 规则 | 说明 |
|------|------|
| 仅 DS 视图自动轮询 | `displayView == DeepSeek` 才周期请求 |
| 默认间隔 | **180s**（可配置 30–3600） |
| 无 Key | 不发 HTTP；TFT 提示配置 API Key（串口/Web） |
| 持久化 | 成功拉取后写入 NVS；开机/切回 DS 先显示缓存 |
| 强制刷新 | 串口 `ds refresh` / Web 按钮可随时拉一次并写 NVS |

## 非目标（本轮）

登录、HTTPS、Captive Portal 强制跳转、网页历史曲线、OTA。

## 主要改动文件

- `src/net/wifi_manager.*` — AP 逻辑  
- `src/net/web_ui.*` — 新建  
- `src/net/deepseek_monitor.*` — DS-only / persist / 180s  
- `src/storage/settings_store.*` — 余额缓存 NVS  
- `src/display/display_driver.*` — 无 Key 提示  
- `src/main.cpp` / `src/cli/serial_cli.*` / 文档  
