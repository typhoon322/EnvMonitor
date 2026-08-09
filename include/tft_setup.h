// TFT_eSPI custom setup for ST7789 1.47" 172x320 panel (used in landscape as 320x172).
//
// TFT_eSPI >= 2.5.x auto-includes a file named "tft_setup.h" from the sketch
// include path (see TFT_eSPI.h: __has_include(<tft_setup.h>)). The old
// TFT_ESPI_USER_SETUP_PATH build flag is NOT supported by this library version,
// so this file is how the pins/driver are selected.
//
// Must stay in sync with the TFT pins documented in board_c3_pro_mini.h.

#define USER_SETUP_INFO "ESP32-C3 Pro Mini + ST7789 1.47in 172x320"

#define ST7789_DRIVER  // ST7789 controller

// 1.47" panels are BGR colour order (ST7789_Defines.h defaults to BGR when
// CGRAM_OFFSET is active, so TFT_RGB_ORDER is not required here).

// Native panel resolution (portrait); rotation 1 is used in firmware -> 320x172
#define TFT_WIDTH 172
#define TFT_HEIGHT 320

// The controller memory is 240x320; a 172-wide panel needs the CGRAM offset.
// ST7789_Defines.h enables CGRAM_OFFSET automatically for 172x320 panels.
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

// Required: with USER_SETUP_LOADED the library skips User_Setup.h, so fonts
// must be selected here. Without LOAD_GLCD, fillScreen works but print() is a
// no-op (cyan flash then blank screen).
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// TFT_eSPI skips the driver-define chain inside User_Setup_Select.h when a
// sketch-level tft_setup.h exists (USER_SETUP_LOADED). Pull in the ST7789
// command constants and CGRAM_OFFSET/colour-order logic ourselves.
#include "TFT_Drivers/ST7789_Defines.h"
