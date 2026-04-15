/**
 * @file color_calibration.h
 * @brief Display Color Calibration Interface
 * 
 * Color calibration for ST7789 displays with non-standard RGB mappings.
 * 
 * Features:
 * - Multiple color mapping profiles
 * - NVS persistence
 * - Web interface integration
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 */

#ifndef COLOR_CALIBRATION_H
#define COLOR_CALIBRATION_H

#include <stdint.h>
#include "esp_err.h"

// Color mapping profile types
typedef enum {
    COLOR_PROFILE_STANDARD = 0,     // Standard RGB565 mapping
    COLOR_PROFILE_INVERTED,         // Inverted colors
    COLOR_PROFILE_BGR,              // BGR instead of RGB
    COLOR_PROFILE_CUSTOM_1,         // Custom mapping 1 (based on testing)
    COLOR_PROFILE_CUSTOM_2,         // Custom mapping 2 (based on testing)
    COLOR_PROFILE_CUSTOM_3,         // Custom mapping 3 (based on testing)
    COLOR_PROFILE_MAX               // Total number of profiles
} color_profile_t;

// Color format types for testing different encodings
typedef enum {
    COLOR_FORMAT_RGB565 = 0,        // Standard RGB565 (5-6-5 bits)
    COLOR_FORMAT_RGB555,            // RGB555 (5-5-5 bits)
    COLOR_FORMAT_BGR565,            // BGR565 (Blue-Green-Red)
    COLOR_FORMAT_BGR555,            // BGR555 (Blue-Green-Red 5-5-5)
    COLOR_FORMAT_RGB332,            // RGB332 (3-3-2 bits)
    COLOR_FORMAT_BGR332,            // BGR332 (Blue-Green-Red 3-3-2)
    COLOR_FORMAT_GRAYSCALE,         // 8-bit grayscale
    COLOR_FORMAT_MAX                // Total number of formats
} color_format_t;

// Standard RGB565 color definitions
#define STD_COLOR_BLACK   0x0000
#define STD_COLOR_RED     0xF800
#define STD_COLOR_GREEN   0x07E0
#define STD_COLOR_BLUE    0x001F
#define STD_COLOR_YELLOW  0xFFE0
#define STD_COLOR_MAGENTA 0xF81F
#define STD_COLOR_CYAN    0x07FF
#define STD_COLOR_WHITE   0xFFFF

// Initialize color calibration system
esp_err_t color_calibration_init(void);

// Load calibration from NVS
esp_err_t color_calibration_load(void);

// Save current calibration to NVS
esp_err_t color_calibration_save(void);

// Get current color profile
color_profile_t color_calibration_get_profile(void);

// Set color profile
void color_calibration_set_profile(color_profile_t profile);

// Cycle to next color profile
color_profile_t color_calibration_cycle_profile(void);

// Convert standard RGB565 color to display-specific color using current profile
uint16_t color_calibration_convert(uint16_t standard_color);

// Get profile name for display
const char* color_calibration_get_profile_name(color_profile_t profile);

// Test current profile by setting display to a test color
void color_calibration_test_color(uint16_t standard_color);

// Reset to factory defaults
void color_calibration_reset_to_defaults(void);

// Save custom red mapping (for RGB slider interface)
esp_err_t color_calibration_save_custom_red_mapping(uint16_t red_rgb565);

// Color format functions
color_format_t color_calibration_get_format(void);
void color_calibration_set_format(color_format_t format);
color_format_t color_calibration_cycle_format(void);
const char* color_calibration_get_format_name(color_format_t format);

// Convert RGB888 to different color formats
uint16_t color_calibration_convert_rgb888_to_format(uint8_t r, uint8_t g, uint8_t b, color_format_t format);

// Test color with specific format
void color_calibration_test_color_format(uint8_t r, uint8_t g, uint8_t b, color_format_t format);

// Display color persistence functions
esp_err_t color_calibration_save_display_color(uint16_t color);
uint16_t color_calibration_load_display_color(void);
esp_err_t color_calibration_set_default_display_color(uint16_t color);

#endif // COLOR_CALIBRATION_H
