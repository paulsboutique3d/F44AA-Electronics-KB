/**
 * @file ws2812.h
 * @brief WS2812 Addressable LED Interface
 * 
 * Muzzle flash and torch LED control.
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

// WS2812 LED control functions
void ws2812_init(void);
void ws2812_flash(void);

// Torch (tactical light) control functions
void ws2812_torch_set_state(bool enabled);
void ws2812_torch_set_color(uint8_t red, uint8_t green, uint8_t blue);
bool ws2812_torch_get_state(void);
void ws2812_torch_get_color(uint8_t *red, uint8_t *green, uint8_t *blue);
