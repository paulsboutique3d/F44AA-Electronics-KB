/**
 * @file main.c
 * @brief F44AA Pulse Rifle - Main Application Entry Point
 * 
 * Main application for the F44AA Pulse Rifle replica. Initializes all
 * hardware subsystems and manages the main control loop including:
 * - Trigger detection and firing logic
 * - Ammo counter display management
 * - Camera feed and AI person detection
 * - Web server for configuration
 * - Audio playback via DFPlayer Pro
 * - Bluetooth audio transmission
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 * 
 * Hardware: Freenove ESP32-S3 WROOM CAM
 * Display: Dual ST7789 1.9" 170x320 IPS
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"

#include "trigger.h"
#include "magazine.h"
#include "ws2812.h"
#include "dfplayer.h"
#include "counter_display.h"
#include "color_calibration.h"
#include "webserver.h"
#include "ota_update.h"
#include "wifi_config.h"
#include "camera.h"
#include "bluetooth_transmitter.h"
#include "camera_display.h"
#include "ai_targeting.h"

static const char *TAG = "F44AA";

// Task handles (non-static: accessed by webserver)
TaskHandle_t camera_display_task_handle = NULL;
TaskHandle_t person_detection_task_handle = NULL;

// Person detection task - runs AI inference on camera frames at ~5 FPS
void person_detection_task(void *pvParameter) {
    ESP_LOGI(TAG, "Person detection task started");
    camera_fb_t *fb = NULL;

    while (1) {
        if (!camera_is_initialized()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        camera_err_t cap_ret = camera_capture(&fb);
        if (cap_ret != CAMERA_OK || fb == NULL) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // AI targeting requires RGB565; skip JPEG frames
        if (fb->format == PIXFORMAT_RGB565) {
            esp_err_t ret = ai_targeting_process_frame(fb->buf, fb->width, fb->height, fb->format);
            if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(TAG, "AI targeting process failed: %s", esp_err_to_name(ret));
            }
        }

        camera_return_fb(fb);
        fb = NULL;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// Start camera display task
void camera_display_start(void) {
    if (camera_display_task_handle != NULL) {
        ESP_LOGD(TAG, "Camera display task already running");
        return;
    }
    
    BaseType_t result = xTaskCreatePinnedToCore(
        camera_display_task,
        "camera_display",
        5120,
        NULL,
        3,
        &camera_display_task_handle,
        0
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create camera display task");
        camera_display_task_handle = NULL;
    } else {
        ESP_LOGI(TAG, "Camera display task started");
    }
}

// Stop camera display task
void camera_display_stop(void) {
    if (camera_display_task_handle == NULL) {
        ESP_LOGD(TAG, "Camera display task not running");
        return;
    }
    
    // Clean shutdown
    vTaskDelete(camera_display_task_handle);
    camera_display_task_handle = NULL;
    camera_display_deinit();
}

#define FIRE_RATE_RPM 900
#define FIRE_DELAY_MS (60000 / FIRE_RATE_RPM)
#define TRIGGER_POLL_MS 10
#define MAGAZINE_CHECK_MS 50

// Full auto firing task
void trigger_task(void *pvParameter) {
    ESP_LOGI(TAG, "Trigger task started - Full Auto Mode (%d RPM)", FIRE_RATE_RPM);

    esp_err_t wdt_result = esp_task_wdt_delete(NULL);
    if (wdt_result != ESP_OK) {
        ESP_LOGW(TAG, "Could not remove task from watchdog, will reset it instead");
        esp_task_wdt_add(NULL);
    }

    bool was_trigger_pressed = false;
    bool last_magazine_state = magazine_is_inserted();
    uint32_t magazine_check_counter = 0;
    uint32_t debug_log_counter = 0;

    while (1) {
        if (wdt_result != ESP_OK) {
            esp_task_wdt_reset();
        }

        if (magazine_check_counter++ >= (MAGAZINE_CHECK_MS / TRIGGER_POLL_MS)) {
            magazine_check_counter = 0;

            if (magazine_check_reload_event()) {
                if (counter_display_can_reload()) {
                    ESP_LOGI(TAG, "Magazine reload detected");
                    counter_display_reset_ammo();
                    dfplayer_play_reload();
                    ws2812_flash();
                } else {
                    ESP_LOGW(TAG, "Magazine inserted but reload denied - not empty");
                }
            }

            bool current_magazine_state = magazine_is_inserted();
            if (!magazine_is_test_mode() && last_magazine_state && !current_magazine_state && !counter_display_is_empty()) {
                ESP_LOGI(TAG, "Magazine removed - setting ammo to 0");
                counter_display_set_empty();
            }
            last_magazine_state = current_magazine_state;
        }

        bool trigger_pressed = trigger_fired();

        if (trigger_pressed) {
            was_trigger_pressed = true;

            if (!magazine_is_inserted()) {
                if (debug_log_counter++ % 100 == 0) {
                    ESP_LOGW(TAG, "Trigger pulled but no magazine");
                }
                vTaskDelay(pdMS_TO_TICKS(TRIGGER_POLL_MS));
                taskYIELD();
                continue;
            }

            if (counter_display_is_empty()) {
                if (debug_log_counter++ % 100 == 0) {
                    ESP_LOGW(TAG, "Trigger pulled but magazine empty");
                }
                ws2812_flash();
                vTaskDelay(pdMS_TO_TICKS(100));
                taskYIELD();
                continue;
            }

            if (debug_log_counter++ % 50 == 0) {
                ESP_LOGI(TAG, "Firing - %d rounds left", counter_display_get_ammo_count());
            }

            ws2812_flash();
            dfplayer_play_fire();
            counter_display_decrement();
            vTaskDelay(pdMS_TO_TICKS(FIRE_DELAY_MS));
            taskYIELD();

        } else {
            if (was_trigger_pressed) {
                ESP_LOGI(TAG, "Trigger released - syncing display");
                counter_display_sync();
                was_trigger_pressed = false;
                debug_log_counter = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(TRIGGER_POLL_MS));
            taskYIELD();
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "F44AA system boot");

    // Initialize NVS (required for WiFi and OTA)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Core weapon systems
    trigger_init();
    magazine_init();
    ws2812_init();
    dfplayer_init();

    esp_err_t display_ret = counter_display_init();
    if (display_ret != ESP_OK) {
        ESP_LOGE(TAG, "Counter display init failed: %s", esp_err_to_name(display_ret));
    }
    counter_display_set_mode(BAR_F44AA);

    if (!magazine_is_inserted()) {
        counter_display_set_empty();
    }

    esp_err_t cal_ret = color_calibration_init();
    if (cal_ret == ESP_OK) {
        uint16_t saved_color = color_calibration_load_display_color();
        counter_display_set_bar_color(saved_color);
        ESP_LOGI(TAG, "Display color restored: 0x%04X", saved_color);
    } else {
        ESP_LOGW(TAG, "Color calibration init failed, using defaults");
    }

    // Secondary systems
    esp_err_t bt_ret = bluetooth_transmitter_init();
    if (bt_ret == ESP_OK) {
        bluetooth_transmitter_enable(false);
        ESP_LOGI(TAG, "Bluetooth transmitter ready");
    } else {
        ESP_LOGW(TAG, "Bluetooth transmitter init failed: %s", esp_err_to_name(bt_ret));
    }

    camera_err_t camera_ret = camera_init();
    if (camera_ret == CAMERA_OK) {
        ESP_LOGI(TAG, "Camera system ready");

        ai_targeting_config_t ai_config = ai_targeting_get_default_config();
        ai_config.mode = AI_TARGETING_MONITOR;
        ai_config.audio_feedback = true;
        ai_config.visual_feedback = true;

        esp_err_t ai_ret = ai_targeting_init(&ai_config);
        if (ai_ret == ESP_OK) {
            ai_targeting_start(AI_TARGETING_MONITOR);
            ESP_LOGI(TAG, "AI targeting system initialized");

            BaseType_t pd_result = xTaskCreatePinnedToCore(
                person_detection_task,
                "person_detect",
                8192,
                NULL,
                2,
                &person_detection_task_handle,
                0
            );
            if (pd_result != pdPASS) {
                ESP_LOGW(TAG, "Failed to create person detection task");
            }
        } else {
            ESP_LOGW(TAG, "AI targeting init failed: %s", esp_err_to_name(ai_ret));
        }

        #ifdef CONFIG_LOG_DEFAULT_LEVEL_DEBUG
        camera_print_info();
        #endif
    } else {
        ESP_LOGW(TAG, "Camera init failed: %d", camera_ret);
    }

    webserver_err_t web_ret = webserver_init();
    if (web_ret == WEBSERVER_OK) {
        ESP_LOGI(TAG, "Web interface: http://%s | AP: F44AA / aliens1986", webserver_get_ip());
    } else {
        ESP_LOGE(TAG, "Web server failed to start");
    }

    BaseType_t result = xTaskCreatePinnedToCore(
        trigger_task,
        "trigger_task",
        3584,
        NULL,
        4,
        NULL,
        1
    );
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create trigger task");
        return;
    }

    ESP_LOGI(TAG, "F44AA READY | %d RPM | Magazine: %s | Ammo: %d/400",
             FIRE_RATE_RPM,
             magazine_is_inserted() ? "LOADED" : "EMPTY",
             counter_display_get_ammo_count());
}