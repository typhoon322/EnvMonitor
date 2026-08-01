# 硬件联调检查清单

烧录固件前建议按顺序完成以下检查。

## 编译与烧录

- [ ] 确认 env：`pio run -e esp32-c3-envmonitor`
- [ ] 编译成功，无 error
- [ ] 生成 `firmware.bin`
- [ ] 烧录与监视：`pio run -e esp32-c3-envmonitor -t upload && pio device monitor`

引脚以 [`include/boards/board_c3_pro_mini.h`](../include/boards/board_c3_pro_mini.h) 为准，对照 [wiring.md](wiring.md)。

## 第一阶段：传感器 + TFT

| 步骤 | 操作 | 预期结果 |
|------|------|----------|
| 1 | 接 ENS160+AHT20 + ILI9341 + C3，USB 供电 | 上电无异常发热 |
| 2 | 烧录并打开串口监视器 | 115200 有输出 |
| 3 | 查看启动日志 | `Board: ESP32-C3-ProMini`；传感器/TFT 初始化成功或 WARN |
| 4 | 查看 TFT 状态页 | 温度、湿度、eCO2、TVOC、AQI |
| 5 | 查看串口 `[status]` 行 | 约 1 Hz；含 T/RH/eCO2/TVOC/AQI/state |
| 6 | 输入 `status` | 返回完整读数 |

**若 I2C 传感器失败：**

- 检查 SDA=8、SCL=9，VCC=3.3V
- 确认模块地址（ENS160 0x53 / 0x52）
- 尝试降低 I2C 速度或交换 SDA/SCL

**若 ENS160 显示 WARMUP / STARTUP：**

- 上电后 3 分钟内为 warm-up，1 小时内为 initial startup，属正常
- 温湿度仍应来自 AHT20

**若 TFT 无显示：**

- 检查 SPI 引脚与 BL（GPIO 3）是否接高
- 确认 VCC 为 3.3V
- 若颜色/方向不对，可在 `display_driver.cpp` 调整 `setRotation()` 或 `invertDisplay()`

## 第二阶段：CLI 与曲线

| 步骤 | 操作 | 预期结果 |
|------|------|----------|
| 1 | `view chart` | TFT 切换为曲线页 |
| 2 | `metric temp` / `hum` / `eco2` | 曲线指标切换 |
| 3 | `chart` 或 `chart 1m` | 步长切换 |
| 4 | 等待 3 分钟以上 | 曲线有数据点 |

## 第三阶段：NVS 设置

| 步骤 | 操作 | 预期结果 |
|------|------|----------|
| 1 | `view chart`、`metric eco2`、`chart 1m` | 设置变更 |
| 2 | `save` | 串口提示 Settings saved |
| 3 | 按 RST 或重新上电 | 视图/步长/指标保持 |
| 4 | `load` | 手动从 NVS 重载 |

## 故障恢复

连续 I2C 读失败时，固件会自动尝试 `recover()`（间隔 ≥30 s）。串口可见 `WARN: attempting sensor recover...`。
