# 串口协议

波特率 **115200**，行尾 `\n`（`\r` 忽略）。

## 周期性状态行

约每 1 秒输出一行：

```
[status] T=23.5C RH=45.2% eCO2=650 TVOC=120 AQI=2 state=OK
```

| 字段 | 说明 |
|------|------|
| T | 温度（°C，AHT20） |
| RH | 相对湿度（%） |
| eCO2 | 等效 CO₂（ppm，ENS160） |
| TVOC | 挥发性有机物（ppb） |
| AQI | 空气质量指数 1–5（Uba） |
| state | `OK` / `WARMUP` / `STARTUP` / `NOTREADY` / `FAULT` / `INIT` |

## CLI 命令

| 命令 | 说明 |
|------|------|
| `help` | 命令列表 |
| `status` | 完整状态（含 view/chart 设置） |
| `view status` | TFT 状态页 |
| `view chart` | TFT 历史曲线页 |
| `chart` | 循环切换步长 3s → 1m → 5m → 15m |
| `chart 3s\|1m\|5m\|15m` | 指定步长 |
| `metric temp\|hum\|eco2` | 曲线 Y 轴指标 |
| `save` | 保存 view/step/metric/backlight 到 NVS |
| `load` | 从 NVS 加载设置 |

命令不区分大小写。未知命令返回提示。

## NVS 持久化字段

namespace: `envmon`

| 键 | 内容 |
|----|------|
| magic | 版本魔数 |
| view | status / chart |
| metric | temp / hum / eco2 |
| cStep | 3s / 1m / 5m / 15m |
| bl | 背光级别 |

启动时自动 `load`；CLI 修改 view/chart/metric 时自动 `save`。
