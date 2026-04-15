/**
 * @file ws2812.c
 * @brief WS2812 Addressable LED Controller
 * 
 * Controls WS2812 addressable LEDs for muzzle flash and torch
 * functionality using ESP-IDF RMT peripheral for precise timing.
 * 
 * Features:
 * - Muzzle flash effect with configurable color
 * - Torch (tactical light) with RGB color control
 * - RMT-based precise timing for WS2812 protocol
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 * 
 * Hardware: WS2812B/SK6812 addressable LEDs
 * GPIO: Muzzle=2, Torch=3
 */

#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// LED strip encoder configuration type
typedef struct {
    uint32_t resolution; /*!< Encoder resolution, in Hz */
} led_strip_encoder_config_t;

// Function prototype
esp_err_t rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder);

#include "pins_config.h"

static const char *TAG = "WS2812";

// Forward declarations
static void ws2812_torch_set_state_internal(bool enabled);

// New RMT TX handles
static rmt_channel_handle_t muzzle_channel = NULL;
static rmt_channel_handle_t torch_channel = NULL;
static rmt_encoder_handle_t led_encoder = NULL;

// Muzzle flash LED
static bool muzzle_initialized = false;

// Torch LED 
static bool torch_initialized = false;

// Torch state
static bool torch_enabled = false;
static uint8_t torch_red = 255;
static uint8_t torch_green = 255;
static uint8_t torch_blue = 255;

// Helper function to build GRB color data for WS2812
void build_ws2812_rgb(uint8_t red, uint8_t green, uint8_t blue, uint8_t *grb_out) {
    grb_out[0] = green;  // WS2812 uses GRB order
    grb_out[1] = red;
    grb_out[2] = blue;
}

void ws2812_torch_set_state(bool enabled) {
    ws2812_torch_set_state_internal(enabled);
}

void ws2812_init(void) {
    ESP_LOGI(TAG, "Initializing WS2812 with new RMT driver - Muzzle Flash on GPIO %d, Torch on GPIO %d", 
             PIN_WS2812_MUZZLE, PIN_WS2812_TORCH);

    // Create LED strip encoder
    led_strip_encoder_config_t encoder_config = {
        .resolution = 10000000,  // 10MHz resolution
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    // Configure RMT TX channel for muzzle flash
    rmt_tx_channel_config_t muzzle_config = {
        .gpio_num = PIN_WS2812_MUZZLE,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,  // 10MHz
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&muzzle_config, &muzzle_channel));
    ESP_ERROR_CHECK(rmt_enable(muzzle_channel));
    muzzle_initialized = true;

    // Configure RMT TX channel for torch
    rmt_tx_channel_config_t torch_config = {
        .gpio_num = PIN_WS2812_TORCH,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,  // 10MHz
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&torch_config, &torch_channel));
    ESP_ERROR_CHECK(rmt_enable(torch_channel));
    torch_initialized = true;

    // Initialize torch to off
    ws2812_torch_set_state_internal(false);

    ESP_LOGI(TAG, "WS2812 initialization complete with new RMT driver");
}

void ws2812_flash(void) {
    if (!muzzle_initialized) {
        ESP_LOGE(TAG, "Muzzle flash not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Muzzle flash fired");
    
    // Orange muzzle flash (R=255, G=50, B=0)
    uint8_t rgb_data[3] = {255, 50, 0};
    
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };
    
    ESP_ERROR_CHECK(rmt_transmit(muzzle_channel, led_encoder, rgb_data, sizeof(rgb_data), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(muzzle_channel, portMAX_DELAY));
    
    vTaskDelay(pdMS_TO_TICKS(150));  // Flash duration

    // Turn off muzzle flash (all zeros)
    uint8_t off_data[3] = {0, 0, 0};
    ESP_ERROR_CHECK(rmt_transmit(muzzle_channel, led_encoder, off_data, sizeof(off_data), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(muzzle_channel, portMAX_DELAY));
}

static void ws2812_torch_set_state_internal(bool enabled) {
    if (!torch_initialized) {
        ESP_LOGE(TAG, "Torch not initialized");
        return;
    }
    
    torch_enabled = enabled;
    
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };
    
    if (enabled) {
        uint8_t rgb_data[3] = {torch_red, torch_green, torch_blue};
        ESP_ERROR_CHECK(rmt_transmit(torch_channel, led_encoder, rgb_data, sizeof(rgb_data), &tx_config));
        ESP_LOGI(TAG, "Torch ON - RGB(%d,%d,%d)", torch_red, torch_green, torch_blue);
    } else {
        uint8_t off_data[3] = {0, 0, 0};
        ESP_ERROR_CHECK(rmt_transmit(torch_channel, led_encoder, off_data, sizeof(off_data), &tx_config));
        ESP_LOGI(TAG, "Torch OFF");
    }
    
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(torch_channel, portMAX_DELAY));
}

void ws2812_torch_set_color(uint8_t red, uint8_t green, uint8_t blue) {
    torch_red = red;
    torch_green = green;
    torch_blue = blue;
    
    // Update torch if it's currently enabled
    if (torch_enabled) {
        ws2812_torch_set_state_internal(true);
    }
    
    ESP_LOGI(TAG, "Torch color set to RGB(%d,%d,%d)", red, green, blue);
}

bool ws2812_torch_get_state(void) {
    return torch_enabled;
}

void ws2812_torch_get_color(uint8_t *red, uint8_t *green, uint8_t *blue) {
    if (red) *red = torch_red;
    if (green) *green = torch_green;
    if (blue) *blue = torch_blue;
}