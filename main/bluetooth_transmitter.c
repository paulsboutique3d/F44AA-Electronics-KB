/**
 * @file bluetooth_transmitter.c
 * @brief KCX-BT_EMITTER Bluetooth Audio Transmitter Driver
 * 
 * Controls the KCX-BT_EMITTER module for wireless audio transmission.
 * Audio is routed from DFPlayer Pro DAC outputs when Line Out mode is selected.
 * 
 * Features:
 * - UART AT command interface for status queries
 * - GPIO KEY pin control for power on/off and pairing
 * - Automatic pairing mode on enable
 * - LED status monitoring
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 * 
 * Hardware: KCX-BT_EMITTER module
 * GPIO: TX=45, RX=46, KEY=36, LED=48
 */

#include "bluetooth_transmitter.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "pins_config.h"

// KCX-BT_EMITTER UART config
#define BT_UART_NUM          UART_NUM_2     // UART2 (UART1 used by DFPlayer)

// KEY pin timing (simulates physical button press)
#define BT_KEY_SHORT_PRESS_MS   150         // Short press = power toggle
#define BT_KEY_LONG_PRESS_MS    3500        // Long press = enter pairing mode

// UART configuration
#define BT_UART_BAUD_RATE    115200         // KCX-BT_EMITTER default baud rate
#define BT_UART_RX_BUFFER    512            // RX buffer size
#define BT_UART_TX_BUFFER    512            // TX buffer size

// AT command strings
#define BT_CMD_TEST          "AT+"
#define BT_CMD_RESET         "AT+RESET"
#define BT_CMD_VERSION       "AT+GMR?"
#define BT_CMD_STATUS        "AT+STATUS?"
#define BT_CMD_DISCONNECT    "AT+DISCON"
#define BT_CMD_SCAN          "AT+PAIR"
#define BT_CMD_POWER_OFF     "AT+POWER_OFF"

// Response timeout
#define BT_RESPONSE_TIMEOUT_MS  2000

static const char *TAG = "BT_TRANSMITTER";
static bool bt_transmitter_initialized = false;
static bool bt_transmitter_enabled = false;

// Send AT command and wait for response
static esp_err_t bt_send_at_command(const char* command, char* response, size_t response_len, uint32_t timeout_ms) {
    if (!bt_transmitter_initialized) {
        ESP_LOGW(TAG, "Bluetooth transmitter not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "Sending AT command: %s", command);
    
    // Clear RX buffer first
    uart_flush(BT_UART_NUM);
    
    // Send command with CRLF
    char cmd_with_crlf[128];
    snprintf(cmd_with_crlf, sizeof(cmd_with_crlf), "%s\r\n", command);
    uart_write_bytes(BT_UART_NUM, cmd_with_crlf, strlen(cmd_with_crlf));
    
    // Wait for response
    if (response && response_len > 0) {
        memset(response, 0, response_len);
        int len = uart_read_bytes(BT_UART_NUM, (uint8_t*)response, response_len - 1, pdMS_TO_TICKS(timeout_ms));
        if (len > 0) {
            response[len] = '\0';
            ESP_LOGD(TAG, "Received response: %s", response);
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "No response received for command: %s", command);
            return ESP_ERR_TIMEOUT;
        }
    }
    
    return ESP_OK;
}

esp_err_t bluetooth_transmitter_init(void) {
    if (bt_transmitter_initialized) {
        ESP_LOGW(TAG, "Bluetooth transmitter already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing KCX-BT_EMITTER Bluetooth transmitter");
    
    // Configure KEY pin for power/pair control (active low - pull high normally)
    gpio_config_t key_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_BT_KEY),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&key_conf);
    gpio_set_level(PIN_BT_KEY, 1);  // Keep KEY high (inactive)
    
    // Configure LED status pin (optional - for monitoring)
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << PIN_BT_LED),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);
    
    // Configure UART for AT commands
    uart_config_t uart_config = {
        .baud_rate = BT_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    esp_err_t ret = uart_param_config(BT_UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = uart_set_pin(BT_UART_NUM, PIN_BT_TX, PIN_BT_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = uart_driver_install(BT_UART_NUM, BT_UART_RX_BUFFER, BT_UART_TX_BUFFER, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }
    
    bt_transmitter_initialized = true;
    
    // Give module time to start up
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Test communication
    char response[64];
    ret = bt_send_at_command(BT_CMD_TEST, response, sizeof(response), BT_RESPONSE_TIMEOUT_MS);
    if (ret == ESP_OK && strstr(response, "OK+")) {
        ESP_LOGI(TAG, "KCX-BT_EMITTER communication established");
        bt_transmitter_enabled = true;
        
        // Get version info
        ret = bt_send_at_command(BT_CMD_VERSION, response, sizeof(response), BT_RESPONSE_TIMEOUT_MS);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "KCX-BT_EMITTER version: %s", response);
        }
    } else {
        ESP_LOGW(TAG, "Failed to establish communication with KCX-BT_EMITTER");
        bt_transmitter_enabled = false;
    }
    
    ESP_LOGI(TAG, "Bluetooth transmitter initialized on UART%d (TX: GPIO%d, RX: GPIO%d, KEY: GPIO%d, LED: GPIO%d)", 
             BT_UART_NUM, PIN_BT_TX, PIN_BT_RX, PIN_BT_KEY, PIN_BT_LED);
    
    return ESP_OK;
}

// Simulate KEY button press via GPIO
static void bt_key_press(uint32_t duration_ms) {
    gpio_set_level(PIN_BT_KEY, 0);  // Press (active low)
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    gpio_set_level(PIN_BT_KEY, 1);  // Release
}

void bluetooth_transmitter_enable(bool enable) {
    if (!bt_transmitter_initialized) {
        ESP_LOGW(TAG, "Bluetooth transmitter not initialized");
        return;
    }
    
    if (enable == bt_transmitter_enabled) {
        return;  // Already in requested state
    }
    
    ESP_LOGI(TAG, "Bluetooth transmitter %s via KEY pin", enable ? "enabling" : "disabling");
    
    // Use KEY pin to toggle power (short press = power toggle)
    // KCX-BT_EMITTER toggles between on/off with each short press
    bt_key_press(BT_KEY_SHORT_PRESS_MS);
    vTaskDelay(pdMS_TO_TICKS(1500));  // Wait for state change
    
    if (enable) {
        // Module should now be powering on - wait for it to initialize
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Verify module is responding
        char response[128];
        esp_err_t ret = bt_send_at_command(BT_CMD_TEST, response, sizeof(response), BT_RESPONSE_TIMEOUT_MS);
        if (ret == ESP_OK && strstr(response, "OK+")) {
            bt_transmitter_enabled = true;
            ESP_LOGI(TAG, "Bluetooth transmitter powered on and ready");
            
            // Enter pairing mode with long press if not already paired
            ESP_LOGI(TAG, "Entering pairing mode...");
            bt_key_press(BT_KEY_LONG_PRESS_MS);
            vTaskDelay(pdMS_TO_TICKS(500));
            ESP_LOGI(TAG, "Bluetooth transmitter ready for audio transmission");
        } else {
            ESP_LOGW(TAG, "Module may still be starting - marked as enabled");
            bt_transmitter_enabled = true;  // Assume it worked
        }
    } else {
        // Module should now be powering off
        bt_transmitter_enabled = false;
        ESP_LOGI(TAG, "Bluetooth transmitter powered off");
    }
}

bool bluetooth_transmitter_is_enabled(void) {
    return bt_transmitter_enabled;
}

bool bluetooth_transmitter_is_connected(void) {
    if (!bt_transmitter_enabled) {
        return false;
    }
    
    char response[64];
    esp_err_t ret = bt_send_at_command(BT_CMD_STATUS, response, sizeof(response), BT_RESPONSE_TIMEOUT_MS);
    
    if (ret == ESP_OK && strstr(response, "STATUS:1")) {
        return true;  // Connected
    }
    
    return false;  // Not connected or error
}

void bluetooth_transmitter_deinit(void) {
    if (!bt_transmitter_initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing Bluetooth transmitter");
    
    // Disable transmitter
    bluetooth_transmitter_enable(false);
    
    // Remove UART driver
    uart_driver_delete(BT_UART_NUM);
    
    // Reset LED pin
    gpio_reset_pin(PIN_BT_LED);
    
    bt_transmitter_initialized = false;
    bt_transmitter_enabled = false;
    
    ESP_LOGI(TAG, "Bluetooth transmitter deinitialized");
}
