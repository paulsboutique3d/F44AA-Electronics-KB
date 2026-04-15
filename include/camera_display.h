/**
 * @file camera_display.h
 * @brief Camera Feed Display Interface
 * 
 * Secondary ST7789 display for camera feed with AI overlay.
 * GPIO: CS=21, DC=33, RST=34, BLK=35
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 */

#ifndef CAMERA_DISPLAY_H
#define CAMERA_DISPLAY_H

#include "esp_err.h"
#include "esp_camera.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the camera display (second IPS screen)
 * 
 * Initializes the ST7789 display on dedicated GPIO pins for camera feed visualization.
 * Uses shared SPI bus with counter display but separate control pins.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t camera_display_init(void);

/**
 * @brief Clear the camera display with specified color
 * 
 * @param color RGB565 color value to fill the display
 */
void camera_display_clear(uint16_t color);

/**
 * @brief Display a camera frame on the screen
 * 
 * Takes a camera frame buffer and displays it on the camera display.
 * Handles format conversion and scaling as needed.
 * 
 * @param fb Camera frame buffer from esp_camera_fb_get()
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t camera_display_show_frame(camera_fb_t* fb);

/**
 * @brief Show startup message on camera display
 * 
 * Displays F44AA camera initialization screen
 */
void camera_display_show_startup(void);

/**
 * @brief Show camera status message
 * 
 * @param status Status string to display (e.g., "Camera Ready", "Error", etc.)
 */
void camera_display_show_status(const char* status);

/**
 * @brief Check if camera display is initialized
 * 
 * @return true if initialized, false otherwise
 */
bool camera_display_is_initialized(void);

/**
 * @brief Deinitialize camera display
 * 
 * Cleans up resources and turns off display
 */
void camera_display_deinit(void);

/**
 * @brief Camera display task function
 * 
 * FreeRTOS task that continuously displays camera frames on the second display.
 * Handles initialization, frame acquisition, display updates, and error recovery.
 * 
 * @param pvParameters Task parameters (unused)
 */
void camera_display_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif // CAMERA_DISPLAY_H
