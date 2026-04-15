/**
 * @file camera_display.c
 * @brief Camera Feed Display Module for F44AA Pulse Rifle
 * 
 * Manages the secondary ST7789 display that shows the live camera feed
 * with AI targeting overlay. Shares SPI bus with the counter display.
 * 
 * Features:
 * - Live camera feed display (170x320 portrait mode)
 * - AI targeting overlay with bounding boxes
 * - Target lock indicators
 * - Shared SPI3 bus architecture
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 * 
 * Hardware: ST7789 1.9" 170x320 IPS TFT
 * GPIO: CS=21, DC=33, RST=34, BLK=35, MOSI=1, SCLK=44 (shared)
 */

#include "camera_display.h"
#include "camera.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

// Second display pins (camera feed) - shares SPI bus with counter display
// Using SPI3 bus: shares MOSI (GPIO 1) and SCLK (GPIO 44) with counter display
// Each display has unique CS/DC/RST/BLK pins
#define CAM_DISPLAY_MOSI     1    // SPI Data (shared with counter display)
#define CAM_DISPLAY_SCLK     44   // SPI Clock (shared with counter display)
#define CAM_DISPLAY_CS       21   // Chip Select (unique)
#define CAM_DISPLAY_DC       33   // Data/Command (unique)
#define CAM_DISPLAY_RST      34   // Reset (unique)
#define CAM_DISPLAY_BLK      35   // Backlight/Power (unique)

// Display configuration for camera feed (1.9" 170x320 in portrait mode)
#define CAM_PANEL_WIDTH 170
#define CAM_PANEL_HEIGHT 320
#define CAM_DISPLAY_BUFFER_SIZE (CAM_PANEL_WIDTH * CAM_PANEL_HEIGHT * 2) // RGB565

// ST7789 Commands
#define ST7789_SWRESET     0x01
#define ST7789_SLPOUT      0x11
#define ST7789_COLMOD      0x3A
#define ST7789_MADCTL      0x36
#define ST7789_CASET       0x2A
#define ST7789_RASET       0x2B
#define ST7789_RAMWR       0x2C
#define ST7789_DISPON      0x29

// Colors (RGB565)
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_GRAY    0x8410

static const char *TAG = "CAM_DISPLAY";
static spi_device_handle_t cam_display_spi;
static bool cam_display_initialized = false;
static uint16_t* display_buffer = NULL;

// Send command to camera display
static void cam_display_send_cmd(uint8_t cmd) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    gpio_set_level(CAM_DISPLAY_DC, 0);  // Command mode
    ret = spi_device_transmit(cam_display_spi, &t);
    assert(ret == ESP_OK);
}

// Send data to camera display
static void cam_display_send_data(const uint8_t* data, int len) {
    esp_err_t ret;
    spi_transaction_t t;
    if (len == 0) return;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = data;
    gpio_set_level(CAM_DISPLAY_DC, 1);  // Data mode
    ret = spi_device_transmit(cam_display_spi, &t);
    assert(ret == ESP_OK);
}

// Set display window for camera display
static void cam_display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];
    
    // Column address set
    cam_display_send_cmd(ST7789_CASET);
    data[0] = x0 >> 8;
    data[1] = x0 & 0xFF;
    data[2] = x1 >> 8;
    data[3] = x1 & 0xFF;
    cam_display_send_data(data, 4);
    
    // Row address set
    cam_display_send_cmd(ST7789_RASET);
    data[0] = y0 >> 8;
    data[1] = y0 & 0xFF;
    data[2] = y1 >> 8;
    data[3] = y1 & 0xFF;
    cam_display_send_data(data, 4);
    
    // Memory write
    cam_display_send_cmd(ST7789_RAMWR);
}

// Initialize camera display
esp_err_t camera_display_init(void) {
    if (cam_display_initialized) {
        ESP_LOGW(TAG, "Camera display already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing camera display (ST7789)");
    
    // Configure GPIO pins
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << CAM_DISPLAY_CS) | (1ULL << CAM_DISPLAY_DC) | 
                       (1ULL << CAM_DISPLAY_RST) | (1ULL << CAM_DISPLAY_BLK),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);
    
    // Reset display
    gpio_set_level(CAM_DISPLAY_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(CAM_DISPLAY_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Enable backlight
    gpio_set_level(CAM_DISPLAY_BLK, 1);
    
    // Configure SPI device (shares bus with counter display)
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 80 * 1000 * 1000,  // Increased from 26MHz to 80MHz for faster transfers
        .mode = 0,                            // SPI mode 0
        .spics_io_num = CAM_DISPLAY_CS,
        .queue_size = 8,                      // Increased queue size for better performance
        .flags = SPI_DEVICE_HALFDUPLEX,       // Half-duplex mode for better performance
    };
    
    esp_err_t ret = spi_bus_add_device(SPI3_HOST, &devcfg, &cam_display_spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add camera display SPI device: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize ST7789 display
    cam_display_send_cmd(ST7789_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));
    
    cam_display_send_cmd(ST7789_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Color mode - 16-bit RGB565
    cam_display_send_cmd(ST7789_COLMOD);
    uint8_t colmod = 0x55;  // 16-bit RGB565
    cam_display_send_data(&colmod, 1);
    
    // Memory access control - portrait mode for 1.14" 240x135 display
    cam_display_send_cmd(ST7789_MADCTL);
    uint8_t madctl = 0x00;  // Portrait mode (no rotation/mirroring)
    cam_display_send_data(&madctl, 1);
    
    // Display on
    cam_display_send_cmd(ST7789_DISPON);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Allocate display buffer
    display_buffer = heap_caps_malloc(CAM_DISPLAY_BUFFER_SIZE, MALLOC_CAP_DMA);
    if (display_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate camera display buffer");
        return ESP_ERR_NO_MEM;
    }
    
    cam_display_initialized = true;
    ESP_LOGI(TAG, "Camera display initialized successfully");
    
    // Show startup message
    camera_display_show_startup();
    
    return ESP_OK;
}

// Clear camera display - optimized version
void camera_display_clear(uint16_t color) {
    if (!cam_display_initialized) return;
    
    // Set full screen window
    cam_display_set_window(0, 0, CAM_PANEL_WIDTH - 1, CAM_PANEL_HEIGHT - 1);
    gpio_set_level(CAM_DISPLAY_DC, 1);  // Data mode
    
    // Use chunk-based approach for better performance
    const size_t chunk_lines = 40; // Process 40 lines at once
    const size_t chunk_size = CAM_PANEL_WIDTH * chunk_lines;
    
    // Fill chunk of buffer with color
    for (int i = 0; i < chunk_size && i < (CAM_PANEL_WIDTH * CAM_PANEL_HEIGHT); i++) {
        display_buffer[i] = color;
    }
    
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.tx_buffer = display_buffer;
    
    // Send display in optimized chunks
    int lines_remaining = CAM_PANEL_HEIGHT;
    while (lines_remaining > 0) {
        int lines_this_chunk = (lines_remaining >= chunk_lines) ? chunk_lines : lines_remaining;
        size_t pixels_this_chunk = CAM_PANEL_WIDTH * lines_this_chunk;
        
        t.length = pixels_this_chunk * 16; // pixels * 16 bits
        esp_err_t ret = spi_device_transmit(cam_display_spi, &t);
        assert(ret == ESP_OK);
        
        lines_remaining -= lines_this_chunk;
    }
}

// Display camera frame
esp_err_t camera_display_show_frame(camera_fb_t* fb) {
    if (!cam_display_initialized) {
        ESP_LOGE(TAG, "Camera display not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (fb == NULL || fb->buf == NULL || fb->len == 0) {
        ESP_LOGE(TAG, "Invalid frame buffer");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGD(TAG, "Displaying camera frame: %dx%d, format:%d, size:%zu bytes", 
             fb->width, fb->height, fb->format, fb->len);
    
    // For now, just show a placeholder indicating camera data received
    // TODO: Implement proper frame scaling and format conversion
    camera_display_clear(COLOR_GRAY);
    
    // Draw simple indicator that camera is active
    uint16_t center_x = CAM_PANEL_WIDTH / 2;
    uint16_t center_y = CAM_PANEL_HEIGHT / 2;
    
    // Draw a simple cross pattern to indicate camera activity
    for (int i = center_x - 20; i < center_x + 20; i++) {
        if (i >= 0 && i < CAM_PANEL_WIDTH) {
            display_buffer[center_y * CAM_PANEL_WIDTH + i] = COLOR_WHITE;
        }
    }
    for (int i = center_y - 20; i < center_y + 20; i++) {
        if (i >= 0 && i < CAM_PANEL_HEIGHT) {
            display_buffer[i * CAM_PANEL_WIDTH + center_x] = COLOR_WHITE;
        }
    }
    
    // Update display
    cam_display_set_window(0, 0, CAM_PANEL_WIDTH - 1, CAM_PANEL_HEIGHT - 1);
    gpio_set_level(CAM_DISPLAY_DC, 1);  // Data mode
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = CAM_DISPLAY_BUFFER_SIZE * 8;
    t.tx_buffer = display_buffer;
    spi_device_transmit(cam_display_spi, &t);
    
    return ESP_OK;
}

// Show startup message
void camera_display_show_startup(void) {
    if (!cam_display_initialized) return;
    
    ESP_LOGI(TAG, "Showing camera display startup screen");
    
    // Clear to black
    camera_display_clear(COLOR_BLACK);
    
    // Draw simple text pattern indicating "F44AA CAMERA"
    // This is a simple pixel pattern - in a full implementation you'd use a font
    // Adjusted for 135x240 portrait display
    uint16_t start_x = 17;
    uint16_t start_y = 80;
    
    // Draw some simple rectangular patterns to represent text
    // "F44AA"
    for (int y = start_y; y < start_y + 30; y++) {
        for (int x = start_x; x < start_x + 100; x++) {
            if ((x - start_x) % 20 < 15 && (y - start_y) % 8 < 6) {
                display_buffer[y * CAM_PANEL_WIDTH + x] = COLOR_GREEN;
            }
        }
    }
    
    // "CAMERA" 
    start_y += 50;
    for (int y = start_y; y < start_y + 30; y++) {
        for (int x = start_x; x < start_x + 100; x++) {
            if ((x - start_x) % 17 < 12 && (y - start_y) % 8 < 6) {
                display_buffer[y * CAM_PANEL_WIDTH + x] = COLOR_BLUE;
            }
        }
    }
    
    // Update display
    cam_display_set_window(0, 0, CAM_PANEL_WIDTH - 1, CAM_PANEL_HEIGHT - 1);
    gpio_set_level(CAM_DISPLAY_DC, 1);  // Data mode
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = CAM_DISPLAY_BUFFER_SIZE * 8;
    t.tx_buffer = display_buffer;
    spi_device_transmit(cam_display_spi, &t);
}

// Show camera status
void camera_display_show_status(const char* status) {
    if (!cam_display_initialized) return;
    
    ESP_LOGI(TAG, "Camera status: %s", status);
    
    // Clear to dark gray
    camera_display_clear(COLOR_GRAY);
    
    // Draw status indicator (simple colored rectangle)
    uint16_t color = COLOR_RED;  // Default to red for error
    if (strstr(status, "ready") || strstr(status, "ok")) {
        color = COLOR_GREEN;
    } else if (strstr(status, "init")) {
        color = COLOR_YELLOW;
    }
    
    // Draw status bar at top
    for (int y = 10; y < 30; y++) {
        for (int x = 10; x < CAM_PANEL_WIDTH - 10; x++) {
            display_buffer[y * CAM_PANEL_WIDTH + x] = color;
        }
    }
    
    // Update display
    cam_display_set_window(0, 0, CAM_PANEL_WIDTH - 1, CAM_PANEL_HEIGHT - 1);
    gpio_set_level(CAM_DISPLAY_DC, 1);  // Data mode
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = CAM_DISPLAY_BUFFER_SIZE * 8;
    t.tx_buffer = display_buffer;
    spi_device_transmit(cam_display_spi, &t);
}

// Check if camera display is initialized
bool camera_display_is_initialized(void) {
    return cam_display_initialized;
}

// Deinitialize camera display
void camera_display_deinit(void) {
    if (!cam_display_initialized) return;
    
    ESP_LOGI(TAG, "Deinitializing camera display");
    
    // Turn off backlight
    gpio_set_level(CAM_DISPLAY_BLK, 0);
    
    // Free display buffer
    if (display_buffer) {
        heap_caps_free(display_buffer);
        display_buffer = NULL;
    }
    
    // Remove SPI device
    if (cam_display_spi) {
        spi_bus_remove_device(cam_display_spi);
        cam_display_spi = NULL;
    }
    
    cam_display_initialized = false;
    ESP_LOGI(TAG, "Camera display deinitialized");
}

// Camera display task - handles continuous camera frame display
void camera_display_task(void *pvParameters) {
    ESP_LOGI(TAG, "Camera display task starting...");
    
    // Initialize camera display
    esp_err_t ret = camera_display_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize camera display: %s", esp_err_to_name(ret));
        camera_display_show_status("Init Failed");
        vTaskDelete(NULL);
        return;
    }
    
    camera_display_show_status("Waiting for camera...");
    
    // Wait for camera to be initialized
    int camera_wait_attempts = 0;
    while (!camera_is_initialized() && camera_wait_attempts < 30) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        camera_wait_attempts++;
        ESP_LOGW(TAG, "Waiting for camera initialization... (%d/30)", camera_wait_attempts);
    }
    
    if (!camera_is_initialized()) {
        ESP_LOGE(TAG, "Camera not available after 30 seconds - showing error status");
        camera_display_show_status("Camera Error");
        // Keep task alive but show error status
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            camera_display_show_status("Camera Error");
        }
    }
    
    ESP_LOGI(TAG, "Camera available - starting frame display loop");
    camera_display_show_status("Camera Ready");
    vTaskDelay(pdMS_TO_TICKS(2000));  // Show ready status for 2 seconds
    
    uint32_t frame_count = 0;
    uint32_t error_count = 0;
    TickType_t last_fps_time = xTaskGetTickCount();
    
    while (true) {
        // Check if camera display is still enabled in config
        // Note: We would need access to webserver config here
        // For now, assume it's controlled by task creation/deletion
        
        // Try to get camera frame
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb == NULL) {
            error_count++;
            ESP_LOGW(TAG, "Failed to get camera frame (error count: %lu)", error_count);
            
            if (error_count > 10) {
                camera_display_show_status("Camera Timeout");
                vTaskDelay(pdMS_TO_TICKS(1000));
                error_count = 0;  // Reset error count
            } else {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            continue;
        }
        
        // Display the frame
        ret = camera_display_show_frame(fb);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to display frame: %s", esp_err_to_name(ret));
        }
        
        // Return frame buffer
        esp_camera_fb_return(fb);
        
        frame_count++;
        error_count = 0;  // Reset error count on successful frame
        
        // Log FPS every 5 seconds
        TickType_t current_time = xTaskGetTickCount();
        if ((current_time - last_fps_time) >= pdMS_TO_TICKS(5000)) {
            float fps = (float)frame_count * 1000.0f / (float)pdTICKS_TO_MS(current_time - last_fps_time);
            ESP_LOGI(TAG, "Camera display FPS: %.1f (frames: %lu)", fps, frame_count);
            frame_count = 0;
            last_fps_time = current_time;
        }
        
        // Control frame rate - aim for ~10 FPS to avoid overwhelming the display
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
