/**
 * @file magazine.c
 * @brief Magazine Detection and Ammo Management
 * 
 * Handles magazine insertion detection via Hall effect sensor
 * and manages ammunition count for the F44AA Pulse Rifle.
 * 
 * Features:
 * - Hall effect sensor magazine detection
 * - 400 round capacity (movie accurate)
 * - Automatic reload detection
 * - Test mode override for web interface
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 * 
 * Hardware: A3144 Hall Effect Sensor
 * GPIO: 38 (active low)
 */

#include "magazine.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define MAGAZINE_PIN GPIO_NUM_38  // ESP32-S3-CAM magazine well sensor pin (corrected from GPIO 1)

static const char *TAG = "MAGAZINE";
static bool magazine_inserted = false;
static bool last_magazine_state = false;
static bool test_mode_active = false;  // Flag to indicate test mode override

void magazine_init(void) {
    ESP_LOGI(TAG, "Initializing magazine sensor on GPIO %d", MAGAZINE_PIN);
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MAGAZINE_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // Read initial state
    magazine_inserted = gpio_get_level(MAGAZINE_PIN) == 0; // Assuming active LOW
    last_magazine_state = magazine_inserted;
    
    ESP_LOGI(TAG, "Magazine sensor initialized - State: %s", 
             magazine_inserted ? "INSERTED" : "REMOVED");
}

bool magazine_is_inserted(void) {
    return magazine_inserted;
}

bool magazine_check_reload_event(void) {
    // If in test mode, don't check physical sensor
    if (test_mode_active) {
        return false;  // No reload events during test mode
    }
    
    // Read current magazine state
    bool current_state = gpio_get_level(MAGAZINE_PIN) == 0; // Active LOW
    
    // Check for magazine insertion event (removed -> inserted)
    bool reload_event = false;
    if (!last_magazine_state && current_state) {
        // Magazine was just inserted
        reload_event = true;
        ESP_LOGI(TAG, "MAGAZINE INSERTED - Reload event detected");
    } else if (last_magazine_state && !current_state) {
        // Magazine was just removed
        ESP_LOGI(TAG, "MAGAZINE REMOVED");
    }
    
    // Update states
    magazine_inserted = current_state;
    last_magazine_state = current_state;
    
    return reload_event;
}

// Override function for web interface testing (simulates magazine insertion)
void magazine_set_override(bool inserted) {
    test_mode_active = true;  // Enable test mode
    magazine_inserted = inserted;
    last_magazine_state = inserted;
    ESP_LOGI(TAG, "MAGAZINE STATUS OVERRIDE - Set to: %s (Web Interface Test Mode)", 
             inserted ? "INSERTED" : "REMOVED");
}

// Clear test mode and return to normal sensor operation
void magazine_clear_test_mode(void) {
    test_mode_active = false;
    // Read actual sensor state
    bool current_state = gpio_get_level(MAGAZINE_PIN) == 0;
    magazine_inserted = current_state;
    last_magazine_state = current_state;
    ESP_LOGI(TAG, "Test mode cleared - Returning to physical sensor (State: %s)", 
             magazine_inserted ? "INSERTED" : "REMOVED");
}

// Check if currently in test mode
bool magazine_is_test_mode(void) {
    return test_mode_active;
}

// Check real physical magazine status (bypasses test mode override)
bool magazine_is_physically_inserted(void) {
    return gpio_get_level(MAGAZINE_PIN) == 0; // Active LOW sensor
}