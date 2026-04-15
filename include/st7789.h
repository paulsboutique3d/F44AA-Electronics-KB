/**
 * @file st7789.h
 * @brief Shared ST7789 Display Definitions
 *
 * Common command codes and macros shared between counter_display and
 * camera_display modules to avoid duplication.
 */

#pragma once

#include <stdint.h>

// ST7789 Commands
#define ST7789_SWRESET  0x01
#define ST7789_SLPOUT   0x11
#define ST7789_NORON    0x13
#define ST7789_INVOFF   0x20
#define ST7789_INVON    0x21
#define ST7789_DISPON   0x29
#define ST7789_CASET    0x2A
#define ST7789_RASET    0x2B
#define ST7789_RAMWR    0x2C
#define ST7789_MADCTL   0x36
#define ST7789_COLMOD   0x3A
