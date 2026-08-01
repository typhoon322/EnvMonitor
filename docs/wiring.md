# 接线说明

## BOM

| 序号 | 元件 | 数量 | 备注 |
|------|------|------|------|
| 1 | ESP32-C3 Pro Mini | 1 | USB 供电 / 烧录 |
| 2 | ENS160 + AHT20 模块 | 1 | I2C，常见地址 ENS160=0x53、AHT20=0x38 |
| 3 | ILI9341 TFT 2.4 寸 | 1 | 320×240，4-wire SPI |
| 4 | 杜邦线 | 若干 | 3.3V 逻辑 |

## 引脚定义

引脚在 [`include/boards/board_c3_pro_mini.h`](../include/boards/board_c3_pro_mini.h) 中定义，编译 env 为 `esp32-c3-envmonitor`。

> C3 模组上 **GPIO11–17 通常接 Flash**，不可使用。

| 功能 | GPIO | 说明 |
|------|------|------|
| I2C SDA | 8 | ENS160 + AHT20 |
| I2C SCL | 9 | 400 kHz |
| SPI MOSI | 7 | ILI9341 SDI |
| SPI SCK | 6 | ILI9341 SCK |
| TFT CS | 10 | 片选 |
| TFT DC | 4 | 数据/命令 |
| TFT RST | 5 | 复位 |
| TFT BL | 3 | 背光（高电平亮） |

## ENS160 + AHT20（I2C）

| 模块 | ESP32-C3 |
|------|----------|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 8 |
| SCL | GPIO 9 |

- 默认 ENS160 I2C 地址 **0x53**；若模块 ADDR 接 VCC 则为 **0x52**（需改驱动或模块跳线）
- AHT20 地址 **0x38**
- 两芯片共用一条 I2C 总线

## ILI9341 TFT（SPI）

| TFT | ESP32-C3 |
|-----|----------|
| VCC | 3.3V（勿接 5V） |
| GND | GND |
| CS | GPIO 10 |
| RESET | GPIO 5 |
| DC / RS | GPIO 4 |
| MOSI / SDI | GPIO 7 |
| SCK | GPIO 6 |
| LED / BL | GPIO 3 |
| MISO | 不接（4-wire SPI） |

## 电源

- MCU、传感器、屏幕均使用 **3.3V**
- 所有 GND 共地
- USB 供电即可满足开发调试

## 改引脚

只需编辑 [`include/boards/board_c3_pro_mini.h`](../include/boards/board_c3_pro_mini.h)，重新编译烧录：

```bash
pio run -e esp32-c3-envmonitor -t upload
```

## 烧录

```bash
pio run -e esp32-c3-envmonitor -t upload
pio device monitor
```
