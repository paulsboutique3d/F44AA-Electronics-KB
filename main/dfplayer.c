/**
 * @file dfplayer.c
 * @brief DFPlayer Pro Audio Module Driver
 * 
 * Controls the DFPlayer Pro for sound effects playback. Supports
 * dual audio output modes: internal speaker or line out to Bluetooth.
 * 
 * Features:
 * - Fire and reload sound effects
 * - Volume control (0-30)
 * - Audio mode switching (Speaker/Line Out)
 * - Bluetooth transmitter integration
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 * 
 * Hardware: DFPlayer Pro (DFR0768)
 * GPIO: TX=47, RX=14 (UART1)
 */

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>
#include "dfplayer.h"
#include "bluetooth_transmitter.h"

#define DFPLAYER_UART_NUM      UART_NUM_1
#define DFPLAYER_TX_PIN        GPIO_NUM_47  // ESP32-S3 TX → DFPlayer RX (moved from 15, conflicts with camera XCLK)
#define DFPLAYER_RX_PIN        GPIO_NUM_14  // ESP32-S3 RX ← DFPlayer TX
#define DFPLAYER_BAUD_RATE     9600

static const char *TAG = "DFPlayer_Pro";

static audio_output_mode_t current_audio_mode = AUDIO_OUTPUT_SPEAKER;

// Forward declarations
static void df_send_command(uint8_t cmd, uint16_t param);

// RAW COMMAND STRUCTURE
// Format: 0x7E FF 06 CMD 00 00 xx xx CHK1 CHK2 0xEF
// DFPlayer Pro supports both speaker and line out

static void df_send_command(uint8_t cmd, uint16_t param) {
    uint8_t buffer[10];
    uint16_t checksum = 0xFFFF - (0xFF + 0x06 + cmd + 0x00 + (param >> 8) + (param & 0xFF)) + 1;

    buffer[0] = 0x7E;
    buffer[1] = 0xFF;
    buffer[2] = 0x06;
    buffer[3] = cmd;
    buffer[4] = 0x00;
    buffer[5] = 0x00;
    buffer[6] = (param >> 8);
    buffer[7] = (param & 0xFF);
    buffer[8] = (checksum >> 8);
    buffer[9] = (checksum & 0xFF);

    uart_write_bytes(DFPLAYER_UART_NUM, (const char*)buffer, sizeof(buffer));
    ESP_LOGI(TAG, "Sent DFPlayer CMD 0x%02X Param 0x%04X", cmd, param);
}

void dfplayer_init(void) {
    uart_config_t config = {
        .baud_rate = DFPLAYER_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_driver_install(DFPLAYER_UART_NUM, 256, 0, 0, NULL, 0);
    uart_param_config(DFPLAYER_UART_NUM, &config);
    uart_set_pin(DFPLAYER_UART_NUM, DFPLAYER_TX_PIN, DFPLAYER_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    vTaskDelay(pdMS_TO_TICKS(1000));  // Wait for DFPlayer Pro boot

    ESP_LOGI(TAG, "Initializing DFPlayer Pro with line out capability");
    df_send_command(0x06, 25);  // Set volume (0–30) - Default volume
    df_send_command(0x3F, 0);   // Reset device
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Set initial audio output mode to speaker and configure accordingly
    dfplayer_set_audio_mode(AUDIO_OUTPUT_SPEAKER);
    
    ESP_LOGI(TAG, "DFPlayer Pro ready - Audio output: %s", 
             current_audio_mode == AUDIO_OUTPUT_SPEAKER ? "Speaker" : "Line Out");
}

void dfplayer_play_fire(void) {
    df_send_command(0x03, 1);  // Play file 0001.mp3
    ESP_LOGI(TAG, "Playing F44AA fire sound via line out");
}

// Additional DFPlayer Pro functions
void dfplayer_set_volume(uint8_t volume) {
    if (volume > 30) volume = 30;  // Max volume for DFPlayer Pro
    df_send_command(0x06, volume);
    ESP_LOGI(TAG, "Volume set to %d (line out optimized)", volume);
}

void dfplayer_play_reload(void) {
    df_send_command(0x03, 2);  // Play file 0002.mp3 (reload sound)
    ESP_LOGI(TAG, "Playing reload sound");
}

void dfplayer_stop(void) {
    df_send_command(0x16, 0);  // Stop playback
    ESP_LOGI(TAG, "Audio stopped");
}

// Get current audio output mode
audio_output_mode_t dfplayer_get_audio_mode(void) {
    return current_audio_mode;
}

// Software audio mode switching (web interface control)
void dfplayer_set_audio_mode(audio_output_mode_t mode) {
    current_audio_mode = mode;
    if (mode == AUDIO_OUTPUT_LINE_OUT) {
        // Enable Bluetooth transmitter and disable DFPlayer internal amp
        bluetooth_transmitter_enable(true);
        df_send_command(0x1A, 0x01);  // Disable internal amp/speaker output
        df_send_command(0x06, 25);    // Set volume for line out (higher for external devices)
        ESP_LOGI(TAG, "Audio mode: LINE OUT - Bluetooth transmitter enabled, internal amp disabled");
    } else {
        // Disable Bluetooth transmitter and enable DFPlayer internal amp  
        bluetooth_transmitter_enable(false);
        df_send_command(0x1A, 0x00);  // Enable internal amp/speaker output
        df_send_command(0x06, 20);    // Set volume for speaker (lower for internal speaker)
        ESP_LOGI(TAG, "Audio mode: SPEAKER - Bluetooth transmitter disabled, internal amp enabled");
    }
}
