/**
 * @file bluetooth_transmitter.h
 * @brief KCX-BT_EMITTER Bluetooth Audio Transmitter Interface
 * 
 * Wireless audio transmission via KCX-BT_EMITTER module.
 * GPIO: TX=45, RX=46, KEY=36, LED=48
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 */

#ifndef BLUETOOTH_TRANSMITTER_H
#define BLUETOOTH_TRANSMITTER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the KCX-BT_EMITTER Bluetooth transmitter
 * 
 * Configures UART communication for controlling the Bluetooth audio transmitter module.
 * The transmitter uses AT commands for configuration and control via serial interface.
 * Automatically tests communication and retrieves module version information.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bluetooth_transmitter_init(void);

/**
 * @brief Send play/pause command to Bluetooth transmitter
 * 
 * Note: KCX-BT_EMITTER is an audio transmitter, not a receiver.
 * Play/pause control would typically be handled by the audio source or receiving device.
 */
void bluetooth_transmitter_play_pause(void);

/**
 * @brief Send next track command to Bluetooth transmitter
 * 
 * Note: KCX-BT_EMITTER is an audio transmitter, not a receiver.
 * Track control would typically be handled by the audio source or receiving device.
 */
void bluetooth_transmitter_next_track(void);

/**
 * @brief Send previous track command to Bluetooth transmitter
 * 
 * Note: KCX-BT_EMITTER is an audio transmitter, not a receiver.
 * Track control would typically be handled by the audio source or receiving device.
 */
void bluetooth_transmitter_prev_track(void);

/**
 * @brief Enable/disable Bluetooth transmitter
 * 
 * When enabled, performs module reset and initialization via AT commands.
 * Automatically starts scanning for paired devices for auto-connection.
 * When disabled, disconnects current connections and powers down the module.
 * 
 * @param enable true to enable transmitter, false to disable
 */
void bluetooth_transmitter_enable(bool enable);

/**
 * @brief Check if Bluetooth transmitter is enabled
 * 
 * @return true if enabled and communication established, false otherwise
 */
bool bluetooth_transmitter_is_enabled(void);

/**
 * @brief Get connection status of Bluetooth transmitter
 * 
 * Queries the module via AT+STATUS? command to check if a Bluetooth
 * device is currently connected for audio transmission.
 * 
 * @return true if a device is connected, false otherwise
 */
bool bluetooth_transmitter_is_connected(void);

/**
 * @brief Deinitialize Bluetooth transmitter
 * 
 * Disables the module, removes UART driver, and resets GPIO pins.
 * Should be called before system shutdown or module removal.
 */
void bluetooth_transmitter_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // BLUETOOTH_TRANSMITTER_H
