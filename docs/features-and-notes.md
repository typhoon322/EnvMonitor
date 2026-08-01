# 功能说明与注意事项

## 功能概览

| 能力 | 说明 |
|------|------|
| 环境监测 | AHT20 温湿度 + ENS160 eCO2/TVOC/AQI |
| 本地 TFT | ILI9341 320×240 状态页 + 历史曲线 |
| 串口 CLI | 115200，命令见 [protocol.md](protocol.md) |
| 内存历史 | 3 s 基础采样，多档聚合步长 |
| NVS | 视图、曲线步长、指标、背光 |

**未包含（后续 Phase）：** WiFi Web UI、OTA、USB Host 协议、LittleFS 历史落盘。

## ENS160 预热

| getFlags() | 含义 |
|------------|------|
| 0 | 标准运行，输出有效 |
| 1 | Warm-up（约 3 分钟） |
| 2 | Initial startup（首次上电约 1 小时） |
| 3 | 无有效输出 |

- Warmup 期间 **AHT20 温湿度仍可用**，气体读数可能不稳定
- AQI 在 initial startup 结束后更准确
- TFT 与 `[status]` 的 `state` 字段反映上述状态

## AQI（1–5）

| 值 | 含义（Uba 指数） |
|----|------------------|
| 1 | Excellent |
| 2 | Good |
| 3 | Moderate |
| 4 | Poor |
| 5 | Unhealthy |

TFT 状态页用颜色区分 AQI 等级。

## 采样时序

| 常量 | 默认 | 用途 |
|------|------|------|
| SAMPLE_INTERVAL_MS | 1000 | 传感器读取 |
| DISPLAY_INTERVAL_MS | 1000 | TFT 刷新 |
| CHART_SAMPLE_MS | 3000 | 历史 push |
| STATUS_PRINT_INTERVAL_MS | 1000 | 串口 status |

## 开发约定

1. **引脚** 仅定义在 `include/boards/`，驱动中只用 `PIN_*` 宏
2. **模块边界** 传感器 / 显示 / 历史 / CLI / NVS 各自目录
3. **SystemContext** 向 CLI 注入依赖，避免模块内隐式全局
4. **主循环** 单线程 + WDT，不用 FreeRTOS 任务
5. **文档** 中文；代码标识符英文
6. **编译 env** 固定 `esp32-c3-envmonitor`，勿与 TempControl 其他 env 混烧

## TFT 调试

若屏幕空白或镜像：

- 检查 `PIN_TFT_BL` 与 SPI 接线
- 修改 `display_driver.cpp` 中 `setRotation(0..3)`
- 必要时 `tft.invertDisplay(true)`

## 传感器恢复

连续 8 次无效读数且距上次恢复 ≥30 s，自动重新初始化 I2C 设备。串口输出 `WARN: attempting sensor recover...`。
