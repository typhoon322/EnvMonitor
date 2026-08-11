# EnvMonitor 环境检测系统

基于 **ESP32-C3 Pro Mini** 的室内环境监测固件，使用 **ENS160 + AHT20** 检测温湿度与空气质量，**ILI9341** 2.4 寸 TFT（320×240）本地显示，串口 CLI 配置与监控。

架构与开发约定参考 [TempControl](../TempControl) 温控项目。

## 硬件

| 组件 | 说明 |
|------|------|
| MCU | ESP32-C3 Pro Mini |
| 传感器 | ENS160 + AHT20 组合模块（I2C） |
| 显示 | ST7789 1.47 寸 320×172，4-wire SPI |

详细接线见 [docs/wiring.md](docs/wiring.md)。硬件联调步骤见 [docs/hardware-checklist.md](docs/hardware-checklist.md)。

## 快速开始

### 环境要求

- [PlatformIO](https://platformio.org/)（VS Code 扩展或 CLI）

### 编译与烧录

```bash
cd EnvMonitor
pio run -e esp32-c3-envmonitor
pio run -e esp32-c3-envmonitor -t upload
pio device monitor
```

启动串口会打印 `Board: ESP32-C3-ProMini`。

### 串口 CLI（115200 baud）

```
help                     显示命令列表
status                   完整读数与传感器状态
view status|chart        切换 TFT 状态页 / 历史曲线页
chart [3s|1m|5m|15m]     切换或指定曲线步长
metric temp|hum|eco2     曲线页指标
save / load              手动保存 / 加载 NVS 设置
```

完整命令与 `[status]` 行格式见 [docs/protocol.md](docs/protocol.md)。

## 文档

| 文档 | 内容 |
|------|------|
| [docs/wiring.md](docs/wiring.md) | BOM、引脚、接线 |
| [docs/hardware-checklist.md](docs/hardware-checklist.md) | 分阶段联调清单 |
| [docs/protocol.md](docs/protocol.md) | CLI 与 status 格式 |
| [docs/features-and-notes.md](docs/features-and-notes.md) | ENS160 warmup、AQI、开发约定 |

## 项目结构

```
include/
  config.h                 # 时序、阈值
  boards/board_c3_pro_mini.h
src/
  main.cpp
  sensor/                  # ENS160 + AHT20
  display/                 # ILI9341
  history/                 # 内存历史曲线
  cli/                     # 串口 CLI
  storage/                 # NVS 设置
```

## 板载 SoftAP + WebUI

热点固定 SSID `EnvMonitor` / 密码 `envmonitor`，浏览器打开 `http://192.168.4.1`（或内网 IP）可配置 WiFi / MQTT / DeepSeek，并查看环境数据。TFT 可用 GPIO2 按键循环切换 status / chart / DeepSeek 视图。

## 可选：电脑端 DeepSeek 余额看板

与 ESP32 固件解耦，可单独部署（见 [deepseek-usage/README.md](deepseek-usage/README.md)）。板子本身已支持 DeepSeek 余额展示。

## 后续扩展（未实现）

- Home Assistant MQTT Discovery
- OTA、Host 协议（可参考 TempControl）
