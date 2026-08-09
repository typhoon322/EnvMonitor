#pragma once

// TFT_eSPI custom setup for ST7789 1.47" 172x320 panel (used in landscape as 320x172).
// Selected from platformio.ini via: -D TFT_ESPI_USER_SETUP_PATH=<boards/User_Setup_ST7789_172x320.h>
// Must stay in sync with the TFT pins documented in board_c3_pro_mini.h.

#define ST7789_DRIVER  // ST7789 controller

// 1.47" panels are BGR colour order
#define TFT_RGB_ORDER TFT_BGR

// Native panel resolution (portrait); rotation 1 is used in firmware -> 320x172
#define TFT_WIDTH 172
#define TFT_HEIGHT 320

// The controller memory is 240x320; a 172-wide panel needs the CGRAM offset
// (rotation code applies colstart/rowstart automatically when this is defined)
#define CGRAM_OFFSET

// ESP32-C3 Pro Mini pins (no MISO on 4-wire SPI TFT)
#define TFT_MOSI 7
#define TFT_SCLK 6
#define TFT_CS 10
#define TFT_DC 4
#define TFT_RST 5
#define TFT_MISO -1

// Backlight (level control is done manually in DisplayDriver::setBacklight)
#define TFT_BL 3
#define TFT_BACKLIGHT_ON HIGH

#define SPI_FREQUENCY 27000000
