/**
 * @file camera.h
 * @brief Camera Interface for F44AA Pulse Rifle
 * 
 * Camera initialization and capture functions for OV2640 module.
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Use ESP32-camera component's camera_fb_t
#include "esp_camera.h"

// Camera initialization status
typedef enum {
    CAMERA_OK = 0,
    CAMERA_INIT_FAIL = -1,
    CAMERA_CAPTURE_FAIL = -2,
    CAMERA_NOT_INITIALIZED = -3
} camera_err_t;

// Camera functions
camera_err_t camera_init(void);
camera_err_t camera_capture(camera_fb_t **fb);
void camera_return_fb(camera_fb_t *fb);
bool camera_is_initialized(void);
void camera_deinit(void);
camera_err_t camera_reset(void);  // Reset camera if it gets stuck

// Camera settings
camera_err_t camera_set_quality(int quality);  // 0-63, lower is better
camera_err_t camera_set_brightness(int brightness);  // -2 to 2
camera_err_t camera_set_contrast(int contrast);  // -2 to 2

// Camera info
void camera_print_info(void);

#ifdef __cplusplus
}
#endif
