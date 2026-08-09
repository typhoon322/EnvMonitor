# Pre-build patch for TFT_eSPI on ESP32-C3.
#
# Known upstream issue (espressif/arduino-esp32#10908, Bodmer/TFT_eSPI#3384):
# TFT_eSPI's ESP32-C3 port sets `SPI_PORT = SPI2_HOST` (enum value 1), but in the
# ESP-IDF 5.x headers used by PlatformIO espressif32 v7 the GPSPI2 register base is
# only mapped by REG_SPI_BASE(i) when `i == 2`.  SPI_USER_REG(SPI_PORT) therefore
# resolves to 0x10, so the first `*_spi_user` write faults (Store access fault) and
# the chip reset-loops via the task watchdog.
#
# This script rewrites the one line in the installed library so a clean
# re-install of lib_deps cannot silently reintroduce the crash.

import os

Import("env")

_OLD = "#define SPI_PORT SPI2_HOST"
_NEW = "#define SPI_PORT 2"


def patch_c3_header():
    libdep = env.subst("$PROJECT_LIBDEPS_DIR")
    pioenv = env.subst("$PIOENV")
    if not libdep or not pioenv:
        return
    header = os.path.join(libdep, pioenv, "TFT_eSPI", "Processors", "TFT_eSPI_ESP32_C3.h")
    if not os.path.isfile(header):
        print("[tft_patch] TFT_eSPI_ESP32_C3.h not found, skipping")
        return
    with open(header, "r", encoding="utf-8") as fh:
        text = fh.read()
    if text.find(_NEW) != -1 and text.find(_OLD) == -1:
        print("[tft_patch] TFT_eSPI_ESP32_C3.h already patched")
        return
    if _OLD not in text:
        print("[tft_patch] expected SPI_PORT define not found, skipping")
        return
    with open(header, "w", encoding="utf-8") as fh:
        fh.write(text.replace(_OLD, _NEW, 1))
    print("[tft_patch] patched SPI_PORT SPI2_HOST -> 2 in TFT_eSPI_ESP32_C3.h")


patch_c3_header()
