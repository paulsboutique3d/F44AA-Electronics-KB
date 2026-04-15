/**
 * @file color_calibration.c
 * @brief Display Color Calibration System
 * 
 * Provides comprehensive color calibration for ST7789 displays
 * that may have non-standard RGB565 color mappings.
 * 
 * Features:
 * - Multiple color mapping profiles
 * - Cycle through configurations via web interface
 * - Save/load calibration from NVS
 * - Test colors with immediate visual feedback
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 */

#include "color_calibration.h"
#include "counter_display.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "COLOR_CALIBRATION";
static const char *NVS_NAMESPACE = "color_cal";
static const char *NVS_PROFILE_KEY = "profile";
static const char *NVS_FORMAT_KEY = "format";
static const char *NVS_DISPLAY_COLOR_KEY = "disp_color";

static color_profile_t current_profile = COLOR_PROFILE_STANDARD;
static color_format_t current_format = COLOR_FORMAT_RGB565;
static uint16_t current_display_color = STD_COLOR_RED;  // Default to red
static nvs_handle_t nvs_color_handle = 0;

// Profile names for display
static const char* profile_names[COLOR_PROFILE_MAX] = {
    "Standard RGB",
    "Inverted Colors", 
    "BGR Mode",
    "Custom Profile 1",
    "Custom Profile 2", 
    "Custom Profile 3"
};

// Format names for display
static const char* format_names[COLOR_FORMAT_MAX] = {
    "RGB565 (5-6-5)",
    "RGB555 (5-5-5)",
    "BGR565 (5-6-5)",
    "BGR555 (5-5-5)",
    "RGB332 (3-3-2)",
    "BGR332 (3-3-2)",
    "Grayscale (8-bit)"
};

// Color mapping tables for each profile
static uint16_t color_profiles[COLOR_PROFILE_MAX][8] = {
    // COLOR_PROFILE_STANDARD - Standard RGB565
    {
        0x0000, // BLACK -> BLACK
        0xF800, // RED -> RED
        0x07E0, // GREEN -> GREEN
        0x001F, // BLUE -> BLUE
        0xFFE0, // YELLOW -> YELLOW
        0xF81F, // MAGENTA -> MAGENTA
        0x07FF, // CYAN -> CYAN
        0xFFFF  // WHITE -> WHITE
    },
    // COLOR_PROFILE_INVERTED - Inverted colors
    {
        0xFFFF, // BLACK -> WHITE
        0x001F, // RED -> BLUE (try blue instead, might show as red on your display)
        0xF81F, // GREEN -> MAGENTA
        0xFFE0, // BLUE -> YELLOW
        0x07FF, // YELLOW -> CYAN
        0x07E0, // MAGENTA -> GREEN
        0xF800, // CYAN -> RED
        0x0000  // WHITE -> BLACK
    },
    // COLOR_PROFILE_BGR - Blue-Green-Red instead of RGB
    {
        0x0000, // BLACK -> BLACK
        0x001F, // RED -> BLUE
        0xF800, // GREEN -> RED
        0x07E0, // BLUE -> GREEN
        0xF81F, // YELLOW -> MAGENTA
        0xFFE0, // MAGENTA -> YELLOW
        0x07FF, // CYAN -> CYAN
        0xFFFF  // WHITE -> WHITE
    },
    // COLOR_PROFILE_CUSTOM_1 - Based on user testing feedback
    {
        0x0000, // BLACK -> BLACK
        0x07E0, // RED -> GREEN (user reported red shows as green)
        0x07FF, // GREEN -> CYAN (user reported green shows as cyan)
        0x001F, // BLUE -> BLUE (user reported blue works correctly)
        0xFFE0, // YELLOW -> YELLOW (user reported yellow works correctly)
        0xF81F, // MAGENTA -> MAGENTA (user reported magenta works correctly)
        0x07E0, // CYAN -> GREEN (user reported cyan shows as green)
        0xFFFF  // WHITE -> WHITE (user reported white works correctly)
    },
    // COLOR_PROFILE_CUSTOM_2 - Alternative mapping
    {
        0xFFFF, // BLACK -> WHITE (inverted black)
        0xFFE0, // RED -> YELLOW
        0xF800, // GREEN -> RED
        0xF81F, // BLUE -> MAGENTA
        0x07E0, // YELLOW -> GREEN
        0x001F, // MAGENTA -> BLUE
        0x07FF, // CYAN -> CYAN
        0x0000  // WHITE -> BLACK (inverted white)
    },
    // COLOR_PROFILE_CUSTOM_3 - Another alternative
    {
        0x0000, // BLACK -> BLACK
        0x07FF, // RED -> CYAN
        0xF800, // GREEN -> RED
        0x07E0, // BLUE -> GREEN
        0x001F, // YELLOW -> BLUE
        0xFFE0, // MAGENTA -> YELLOW
        0xF81F, // CYAN -> MAGENTA
        0xFFFF  // WHITE -> WHITE
    }
};

// Map standard color to array index
static int get_color_index(uint16_t standard_color) {
    switch (standard_color) {
        case STD_COLOR_BLACK:   return 0;
        case STD_COLOR_RED:     return 1;
        case STD_COLOR_GREEN:   return 2;
        case STD_COLOR_BLUE:    return 3;
        case STD_COLOR_YELLOW:  return 4;
        case STD_COLOR_MAGENTA: return 5;
        case STD_COLOR_CYAN:    return 6;
        case STD_COLOR_WHITE:   return 7;
        default:               return -1;
    }
}

esp_err_t color_calibration_init(void) {
    ESP_LOGI(TAG, "Initializing color calibration system");
    
    // Open NVS handle (NVS already initialized in main)
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_color_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Load saved profile
    ret = color_calibration_load();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No saved calibration found, using defaults");
        current_profile = COLOR_PROFILE_STANDARD;
    }
    
    ESP_LOGI(TAG, "Color calibration initialized - Profile: %s", 
             color_calibration_get_profile_name(current_profile));
    
    return ESP_OK;
}

esp_err_t color_calibration_load(void) {
    if (nvs_color_handle == 0) {
        ESP_LOGE(TAG, "NVS not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Load profile
    uint8_t profile_value = COLOR_PROFILE_STANDARD;
    size_t required_size = sizeof(profile_value);
    
    esp_err_t ret = nvs_get_blob(nvs_color_handle, NVS_PROFILE_KEY, &profile_value, &required_size);
    if (ret == ESP_OK) {
        if (profile_value < COLOR_PROFILE_MAX) {
            current_profile = (color_profile_t)profile_value;
            ESP_LOGI(TAG, "Loaded color profile: %s", color_calibration_get_profile_name(current_profile));
        } else {
            ESP_LOGW(TAG, "Invalid profile value %d, using standard", profile_value);
            current_profile = COLOR_PROFILE_STANDARD;
        }
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved profile found, using standard");
        current_profile = COLOR_PROFILE_STANDARD;
    } else {
        ESP_LOGE(TAG, "Error loading profile: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Load format
    uint8_t format_value = COLOR_FORMAT_RGB565;
    required_size = sizeof(format_value);
    
    ret = nvs_get_blob(nvs_color_handle, NVS_FORMAT_KEY, &format_value, &required_size);
    if (ret == ESP_OK) {
        if (format_value < COLOR_FORMAT_MAX) {
            current_format = (color_format_t)format_value;
            ESP_LOGI(TAG, "Loaded color format: %s", color_calibration_get_format_name(current_format));
        } else {
            ESP_LOGW(TAG, "Invalid format value %d, using RGB565", format_value);
            current_format = COLOR_FORMAT_RGB565;
        }
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved format found, using RGB565");
        current_format = COLOR_FORMAT_RGB565;
    } else {
        ESP_LOGW(TAG, "Error loading format: %s", esp_err_to_name(ret));
        current_format = COLOR_FORMAT_RGB565;
    }
    
    // Load display color
    uint16_t display_color = STD_COLOR_RED;
    required_size = sizeof(display_color);
    
    ret = nvs_get_blob(nvs_color_handle, NVS_DISPLAY_COLOR_KEY, &display_color, &required_size);
    if (ret == ESP_OK) {
        current_display_color = display_color;
        ESP_LOGI(TAG, "Loaded display color: 0x%04X", current_display_color);
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved display color found, using red");
        current_display_color = STD_COLOR_RED;
    } else {
        ESP_LOGW(TAG, "Error loading display color: %s", esp_err_to_name(ret));
        current_display_color = STD_COLOR_RED;
    }
    
    return ESP_OK;
}

esp_err_t color_calibration_save(void) {
    if (nvs_color_handle == 0) {
        ESP_LOGE(TAG, "NVS not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Save profile
    uint8_t profile_value = (uint8_t)current_profile;
    esp_err_t ret = nvs_set_blob(nvs_color_handle, NVS_PROFILE_KEY, &profile_value, sizeof(profile_value));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error saving profile: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Save format
    uint8_t format_value = (uint8_t)current_format;
    ret = nvs_set_blob(nvs_color_handle, NVS_FORMAT_KEY, &format_value, sizeof(format_value));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error saving format: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_commit(nvs_color_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error committing NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Color settings saved - Profile: %s, Format: %s", 
             color_calibration_get_profile_name(current_profile),
             color_calibration_get_format_name(current_format));
    return ESP_OK;
}

color_profile_t color_calibration_get_profile(void) {
    return current_profile;
}

void color_calibration_set_profile(color_profile_t profile) {
    if (profile < COLOR_PROFILE_MAX) {
        current_profile = profile;
        ESP_LOGI(TAG, "Color profile set to: %s", color_calibration_get_profile_name(current_profile));
    } else {
        ESP_LOGW(TAG, "Invalid profile %d, keeping current profile", profile);
    }
}

color_profile_t color_calibration_cycle_profile(void) {
    current_profile = (color_profile_t)((current_profile + 1) % COLOR_PROFILE_MAX);
    ESP_LOGI(TAG, "Cycled to color profile: %s", color_calibration_get_profile_name(current_profile));
    return current_profile;
}

uint16_t color_calibration_convert(uint16_t standard_color) {
    int color_index = get_color_index(standard_color);
    
    if (color_index >= 0) {
        uint16_t converted = color_profiles[current_profile][color_index];
        ESP_LOGD(TAG, "Converting 0x%04X -> 0x%04X (profile: %s)", 
                 standard_color, converted, color_calibration_get_profile_name(current_profile));
        return converted;
    }
    
    // For non-standard colors, pass through without conversion
    ESP_LOGD(TAG, "Non-standard color 0x%04X passed through", standard_color);
    return standard_color;
}

const char* color_calibration_get_profile_name(color_profile_t profile) {
    if (profile < COLOR_PROFILE_MAX) {
        return profile_names[profile];
    }
    return "Unknown Profile";
}

void color_calibration_test_color(uint16_t standard_color) {
    uint16_t display_color = color_calibration_convert(standard_color);
    ESP_LOGI(TAG, "Testing color 0x%04X -> 0x%04X with profile: %s", 
             standard_color, display_color, color_calibration_get_profile_name(current_profile));
    
    // Apply the test color to the display
    counter_display_set_bar_color(display_color);
}

void color_calibration_reset_to_defaults(void) {
    current_profile = COLOR_PROFILE_STANDARD;
    ESP_LOGI(TAG, "Reset to default profile: %s", color_calibration_get_profile_name(current_profile));
    
    // Save the default profile
    color_calibration_save();
}

esp_err_t color_calibration_save_custom_red_mapping(uint16_t red_rgb565) {
    ESP_LOGI(TAG, "Saving custom red mapping: 0x%04X", red_rgb565);
    
    // Update the CUSTOM_1 profile with the new red mapping
    color_profiles[COLOR_PROFILE_CUSTOM_1][1] = red_rgb565;  // Index 1 is RED
    
    // Switch to CUSTOM_1 profile
    current_profile = COLOR_PROFILE_CUSTOM_1;
    
    // Save to NVS
    esp_err_t err = color_calibration_save();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Custom red mapping saved successfully and activated");
        
        // Apply the color immediately
        counter_display_set_bar_color(red_rgb565);
    } else {
        ESP_LOGE(TAG, "Failed to save custom red mapping: %s", esp_err_to_name(err));
    }
    
    return err;
}

// Color format functions
color_format_t color_calibration_get_format(void) {
    return current_format;
}

void color_calibration_set_format(color_format_t format) {
    if (format < COLOR_FORMAT_MAX) {
        current_format = format;
        ESP_LOGI(TAG, "Color format set to: %s", color_calibration_get_format_name(format));
    }
}

color_format_t color_calibration_cycle_format(void) {
    current_format = (color_format_t)((current_format + 1) % COLOR_FORMAT_MAX);
    ESP_LOGI(TAG, "Cycled to format: %s", color_calibration_get_format_name(current_format));
    return current_format;
}

const char* color_calibration_get_format_name(color_format_t format) {
    if (format < COLOR_FORMAT_MAX) {
        return format_names[format];
    }
    return "Unknown Format";
}

uint16_t color_calibration_convert_rgb888_to_format(uint8_t r, uint8_t g, uint8_t b, color_format_t format) {
    uint16_t result = 0;
    
    switch (format) {
        case COLOR_FORMAT_RGB565:
            // Standard RGB565: RRRRR GGGGGG BBBBB
            result = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            break;
            
        case COLOR_FORMAT_RGB555:
            // RGB555: 0 RRRRR GGGGG BBBBB
            result = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
            break;
            
        case COLOR_FORMAT_BGR565:
            // BGR565: BBBBB GGGGGG RRRRR
            result = ((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3);
            break;
            
        case COLOR_FORMAT_BGR555:
            // BGR555: 0 BBBBB GGGGG RRRRR
            result = ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3);
            break;
            
        case COLOR_FORMAT_RGB332:
            // RGB332: RRRGGGBB (8-bit format, but we'll store in 16-bit)
            result = ((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6);
            break;
            
        case COLOR_FORMAT_BGR332:
            // BGR332: BBBGGGR (8-bit format, but we'll store in 16-bit)
            result = ((b >> 5) << 5) | ((g >> 5) << 2) | (r >> 6);
            break;
            
        case COLOR_FORMAT_GRAYSCALE:
            // Convert to grayscale using standard luminance formula: 0.299*R + 0.587*G + 0.114*B
            result = (uint16_t)((r * 299 + g * 587 + b * 114) / 1000);
            break;
            
        default:
            // Fallback to RGB565
            result = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            break;
    }
    
    ESP_LOGD(TAG, "RGB(%d,%d,%d) -> 0x%04X in format %s", 
             r, g, b, result, color_calibration_get_format_name(format));
    
    return result;
}

void color_calibration_test_color_format(uint8_t r, uint8_t g, uint8_t b, color_format_t format) {
    uint16_t display_color = color_calibration_convert_rgb888_to_format(r, g, b, format);
    
    ESP_LOGI(TAG, "Testing RGB(%d,%d,%d) -> 0x%04X with format: %s", 
             r, g, b, display_color, color_calibration_get_format_name(format));
    
    // Apply the test color to the display
    counter_display_set_bar_color(display_color);
}

// Display color persistence functions
esp_err_t color_calibration_save_display_color(uint16_t color) {
    if (nvs_color_handle == 0) {
        ESP_LOGE(TAG, "NVS not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    current_display_color = color;
    
    esp_err_t ret = nvs_set_blob(nvs_color_handle, NVS_DISPLAY_COLOR_KEY, &current_display_color, sizeof(current_display_color));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error saving display color: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_commit(nvs_color_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error committing display color to NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Display color saved to NVS: 0x%04X", current_display_color);
    return ESP_OK;
}

uint16_t color_calibration_load_display_color(void) {
    return current_display_color;
}

esp_err_t color_calibration_set_default_display_color(uint16_t color) {
    current_display_color = color;
    ESP_LOGI(TAG, "Default display color set to: 0x%04X", current_display_color);
    return ESP_OK;
}
