/**
 * @file camera_esp32s3.c
 * @brief OV2640 Camera Driver for Freenove ESP32-S3 WROOM CAM
 * 
 * Camera initialization and capture functions for the built-in OV2640
 * camera module. Configured for RGB565 format for AI processing.
 * 
 * Features:
 * - QVGA (320x240) RGB565 capture for AI inference
 * - JPEG capture for web streaming
 * - Auto exposure and white balance
 * - Camera reset functionality
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 * 
 * Hardware: OV2640 camera (built into Freenove board)
 * GPIO: XCLK=15, SIOD=4, SIOC=5, D0-D7, VSYNC=6, HREF=7, PCLK=13
 */

#include "camera.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_camera.h"
#include "sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "camera_esp32s3";

// === FREENOVE ESP32-S3 WROOM CAM BOARD ===
// Camera pins are hardware-fixed on this board
#define CAM_PIN_PWDN    -1   // Not used
#define CAM_PIN_RESET   -1   // Not used
#define CAM_PIN_XCLK    15   // External clock
#define CAM_PIN_SIOD    4    // I2C SDA
#define CAM_PIN_SIOC    5    // I2C SCL
#define CAM_PIN_D7      16   // Y9
#define CAM_PIN_D6      17   // Y8
#define CAM_PIN_D5      18   // Y7
#define CAM_PIN_D4      12   // Y6
#define CAM_PIN_D3      10   // Y5
#define CAM_PIN_D2      8    // Y4
#define CAM_PIN_D1      9    // Y3
#define CAM_PIN_D0      11   // Y2
#define CAM_PIN_VSYNC   6    // Vertical sync
#define CAM_PIN_HREF    7    // Horizontal reference
#define CAM_PIN_PCLK    13   // Pixel clock

static bool camera_initialized = false;

camera_err_t camera_init(void) {
    if (camera_initialized) {
        ESP_LOGW(TAG, "Camera already initialized");
        return CAMERA_OK;
    }
    
    ESP_LOGI(TAG, "Initializing ESP32-S3-CAM camera module...");
    ESP_LOGI(TAG, "Using official ESP32-S3-CAM pinout configuration");
    
    // Camera configuration - based on latest ESP32-camera examples
    camera_config_t config = {
        .pin_pwdn     = CAM_PIN_PWDN,
        .pin_reset    = CAM_PIN_RESET,
        .pin_xclk     = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        // ESP32-S3 optimized settings
        .xclk_freq_hz = 20000000,           // 20MHz (ESP32-S3 can handle higher frequencies)
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_RGB565,    // RGB565 for ESP-DL AI inference
        .frame_size = FRAMESIZE_QVGA,        // QVGA 320x240 for AI detection
        .jpeg_quality = 12,                  // Not used for RGB565
        .fb_count = 2,                       // Double buffer for smooth operation
        .fb_location = CAMERA_FB_IN_PSRAM,   // Use PSRAM for frame buffers
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY, // Fill buffers when empty
        
        // I2C configuration
        .sccb_i2c_port = 0,                  // I2C port 0
    };

    ESP_LOGI(TAG, "Camera config: XCLK=%luHz, Format=RGB565, FrameSize=QVGA, FB_Count=%d", 
             config.xclk_freq_hz, config.fb_count);
    
    // Initialize the camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed: 0x%x (%s)", err, esp_err_to_name(err));
        
        // Try fallback configuration with lower settings
        ESP_LOGW(TAG, "Trying fallback configuration (QQVGA, 10MHz)...");
        config.frame_size = FRAMESIZE_QQVGA;  // 160x120
        config.xclk_freq_hz = 10000000;       // 10MHz
        config.fb_count = 1;                  // Single buffer
        
        err = esp_camera_init(&config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Camera fallback initialization failed: 0x%x (%s)", err, esp_err_to_name(err));
            return CAMERA_INIT_FAIL;
        }
        ESP_LOGI(TAG, "Camera initialized with fallback QQVGA configuration");
    } else {
        ESP_LOGI(TAG, "Camera initialized successfully with QVGA configuration");
    }

    // Get sensor and configure for optimal performance
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        // Set optimal sensor settings for OV2640
        s->set_brightness(s, 0);     // -2 to 2
        s->set_contrast(s, 0);       // -2 to 2  
        s->set_saturation(s, 0);     // -2 to 2
        s->set_special_effect(s, 0); // 0 = No Effect
        s->set_whitebal(s, 1);       // Enable white balance
        s->set_awb_gain(s, 1);       // Enable AWB gain
        s->set_wb_mode(s, 0);        // 0 = Auto
        s->set_exposure_ctrl(s, 1);  // Enable exposure control
        s->set_aec2(s, 0);           // Disable AEC sensor
        s->set_ae_level(s, 0);       // -2 to 2
        s->set_aec_value(s, 300);    // 0 to 1200
        s->set_gain_ctrl(s, 1);      // Enable gain control
        s->set_agc_gain(s, 0);       // 0 to 30
        s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
        s->set_bpc(s, 0);            // Disable black pixel cancel
        s->set_wpc(s, 1);            // Enable white pixel cancel
        s->set_raw_gma(s, 1);        // Enable raw gamma
        s->set_lenc(s, 1);           // Enable lens correction
        s->set_hmirror(s, 0);        // Disable horizontal mirror
        s->set_vflip(s, 0);          // Disable vertical flip
        s->set_dcw(s, 1);            // Enable DCW (Downsize EN)
        s->set_colorbar(s, 0);       // Disable color bar
        
        ESP_LOGI(TAG, "Sensor configured with optimal settings");
        ESP_LOGI(TAG, "Sensor PID: 0x%02X", s->id.PID);
    } else {
        ESP_LOGW(TAG, "Failed to get camera sensor handle");
    }

    camera_initialized = true;
    ESP_LOGI(TAG, "ESP32-S3-CAM camera initialization complete");
    
    return CAMERA_OK;
}

bool camera_is_initialized(void) {
    return camera_initialized;
}

camera_err_t camera_capture(camera_fb_t **fb) {
    if (!camera_initialized) {
        ESP_LOGE(TAG, "Camera not initialized");
        return CAMERA_NOT_INITIALIZED;
    }
    
    if (fb == NULL) {
        ESP_LOGE(TAG, "Invalid frame buffer pointer");
        return CAMERA_CAPTURE_FAIL;
    }
    
    // Capture frame using esp_camera_fb_get
    camera_fb_t *frame_buffer = esp_camera_fb_get();
    if (frame_buffer == NULL) {
        ESP_LOGE(TAG, "Camera capture failed - no frame received");
        return CAMERA_CAPTURE_FAIL;
    }
    
    // Validate the frame
    if (frame_buffer->len == 0 || frame_buffer->buf == NULL) {
        ESP_LOGW(TAG, "Invalid frame received - len:%zu buf:%p", frame_buffer->len, frame_buffer->buf);
        esp_camera_fb_return(frame_buffer);
        return CAMERA_CAPTURE_FAIL;
    }
    
    *fb = frame_buffer;
    ESP_LOGD(TAG, "Frame captured: %dx%d, format:%d, size:%zu bytes", 
             frame_buffer->width, frame_buffer->height, frame_buffer->format, frame_buffer->len);
    
    return CAMERA_OK;
}

void camera_return_fb(camera_fb_t *fb) {
    if (fb != NULL) {
        esp_camera_fb_return(fb);
        ESP_LOGD(TAG, "Frame buffer returned");
    }
}

camera_err_t camera_set_quality(int quality) {
    if (!camera_initialized) {
        ESP_LOGE(TAG, "Camera not initialized");
        return CAMERA_NOT_INITIALIZED;
    }
    
    if (quality < 0 || quality > 63) {
        ESP_LOGE(TAG, "Invalid quality value: %d (valid range: 0-63)", quality);
        return CAMERA_CAPTURE_FAIL;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        ESP_LOGE(TAG, "Failed to get camera sensor");
        return CAMERA_CAPTURE_FAIL;
    }
    
    s->set_quality(s, quality);
    ESP_LOGI(TAG, "Camera quality set to %d", quality);
    
    return CAMERA_OK;
}

camera_err_t camera_set_brightness(int brightness) {
    if (!camera_initialized) {
        ESP_LOGE(TAG, "Camera not initialized");
        return CAMERA_NOT_INITIALIZED;
    }
    
    if (brightness < -2 || brightness > 2) {
        ESP_LOGE(TAG, "Invalid brightness value: %d (valid range: -2 to 2)", brightness);
        return CAMERA_CAPTURE_FAIL;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        ESP_LOGE(TAG, "Failed to get camera sensor");
        return CAMERA_CAPTURE_FAIL;
    }
    
    s->set_brightness(s, brightness);
    ESP_LOGI(TAG, "Camera brightness set to %d", brightness);
    
    return CAMERA_OK;
}

camera_err_t camera_set_contrast(int contrast) {
    if (!camera_initialized) {
        ESP_LOGE(TAG, "Camera not initialized");
        return CAMERA_NOT_INITIALIZED;
    }
    
    if (contrast < -2 || contrast > 2) {
        ESP_LOGE(TAG, "Invalid contrast value: %d (valid range: -2 to 2)", contrast);
        return CAMERA_CAPTURE_FAIL;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        ESP_LOGE(TAG, "Failed to get camera sensor");
        return CAMERA_CAPTURE_FAIL;
    }
    
    s->set_contrast(s, contrast);
    ESP_LOGI(TAG, "Camera contrast set to %d", contrast);
    
    return CAMERA_OK;
}

void camera_deinit(void) {
    if (camera_initialized) {
        esp_camera_deinit();
        camera_initialized = false;
        ESP_LOGI(TAG, "Camera deinitialized");
    }
}

camera_err_t camera_reset(void) {
    ESP_LOGI(TAG, "Resetting camera");
    camera_deinit();
    vTaskDelay(pdMS_TO_TICKS(100));  // Small delay
    return camera_init();
}

void camera_print_info(void) {
    if (!camera_initialized) {
        ESP_LOGI(TAG, "Camera: Not initialized");
        return;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        ESP_LOGI(TAG, "Camera: Failed to get sensor info");
        return;
    }
    
    ESP_LOGI(TAG, "=== Camera Information ===");
    ESP_LOGI(TAG, "  Sensor PID: 0x%02X", s->id.PID);
    ESP_LOGI(TAG, "  Sensor VER: 0x%02X", s->id.VER);  
    ESP_LOGI(TAG, "  Sensor MIDL: 0x%02X", s->id.MIDL);
    ESP_LOGI(TAG, "  Sensor MIDH: 0x%02X", s->id.MIDH);
    ESP_LOGI(TAG, "  Pixel Format: %d", s->pixformat);
    ESP_LOGI(TAG, "  Frame Size: %d", s->status.framesize);
    ESP_LOGI(TAG, "  Status: Ready");
    ESP_LOGI(TAG, "=========================");
}
