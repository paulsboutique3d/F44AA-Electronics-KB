/**
 * @file counter_display.h
 * @brief Ammo Counter Display Interface
 * 
 * 6-bar ammo counter display for ST7789 1.9" 170x320 TFT.
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// Display mode enumeration
typedef enum {
    BAR_F44AA = 0,      // F44AA authentic red-only bar display
    BAR_COLOR,          // Color-coded ammo levels
    BAR_CLASSIC         // Classic Aliens movie style
} counter_display_mode_t;

// Display configuration constants
#define COUNTER_MAX_AMMO 400
#define COUNTER_BAR_COUNT 6

// Initialization and configuration
esp_err_t counter_display_init(void);
void counter_display_set_mode(counter_display_mode_t mode);

// Display update functions
void counter_display_draw_bars(void);
void counter_display_sync(void);  // Ensure display accuracy after firing stops

// Ammo management functions
void counter_display_decrement(void);
void counter_display_reset_ammo(void);
void counter_display_force_reset_ammo(void);  // Force reset for testing
void counter_display_set_empty(void);  // Set ammo to 0 (no magazine)

// Status inquiry functions
int counter_display_get_ammo_count(void);
bool counter_display_is_empty(void);
int counter_display_get_ammo_percentage(void);
bool counter_display_can_reload(void);

// Color control functions
void counter_display_set_bar_color(uint16_t color);
