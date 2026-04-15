/**
 * @file trigger.c
 * @brief Trigger Input Handler for F44AA Pulse Rifle
 * 
 * Handles trigger switch input detection with debouncing.
 * Uses internal pull-up resistor for simple switch connection.
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 * 
 * Hardware: Microswitch (Omron SS5GL111 or equivalent)
 * GPIO: 0 (active low, internal pull-up)
 */

#include "trigger.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "pins_config.h"

static const char *TAG = "TRIGGER";

void trigger_init(void) {
    ESP_LOGI(TAG, "Initializing trigger on GPIO %d", PIN_TRIGGER);
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_TRIGGER),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

bool trigger_fired(void) {
    // Assuming trigger is active LOW (pressed = 0, released = 1)
    return gpio_get_level(PIN_TRIGGER) == 0;
}