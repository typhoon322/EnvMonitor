#pragma once

// ESP32-C3 Pro Mini — EnvMonitor default wiring
// Note: GPIO11–17 are typically flash pins on C3 modules — do not use them.

#define BOARD_NAME "ESP32-C3-ProMini"

// I2C — ENS160 (0x53) + AHT20 (0x38)
#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9
#define ENS160_I2C_ADDR 0x53
#define AHT20_I2C_ADDR 0x38

// ILI9341 TFT (4-wire SPI, no MISO)
#define PIN_TFT_MOSI 7
#define PIN_TFT_SCK 6
#define PIN_TFT_CS 10
#define PIN_TFT_DC 4
#define PIN_TFT_RST 5
#define PIN_TFT_BL 3
