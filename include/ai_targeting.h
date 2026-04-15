/**
 * @file ai_targeting.h
 * @brief AI-Powered Targeting System for F44AA Pulse Rifle
 * 
 * Integrates ESP-DL person detection with the camera and weapon systems
 * to provide intelligent targeting capabilities.
 * 
 * Features:
 * - Real-time person detection from camera feed
 * - Smart target acquisition and tracking
 * - Integration with existing weapon controls
 * - Configurable targeting modes and behaviors
 * - Web interface integration for AI status monitoring
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 */

#ifndef AI_TARGETING_H
#define AI_TARGETING_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "person_detection.h"
#include "camera.h"

#ifdef __cplusplus
extern "C" {
#endif

// AI targeting modes
typedef enum {
    AI_TARGETING_OFF = 0,        // AI targeting disabled
    AI_TARGETING_MONITOR,        // Monitor only, no weapon control
    AI_TARGETING_ASSIST,         // Assist with target identification
    AI_TARGETING_AUTO,           // Automatic target acquisition
    AI_TARGETING_TRAINING        // Training mode with visual feedback
} ai_targeting_mode_t;

// Target status
typedef enum {
    TARGET_STATUS_NONE = 0,      // No target detected
    TARGET_STATUS_DETECTED,      // Target detected but not locked
    TARGET_STATUS_LOCKED,        // Target locked and ready
    TARGET_STATUS_LOST,          // Target lost recently
    TARGET_STATUS_ERROR          // Detection error
} target_status_t;

// AI targeting configuration
typedef struct {
    ai_targeting_mode_t mode;           // Current targeting mode
    float detection_threshold;          // Minimum confidence for target (0.0-1.0)
    uint32_t lock_time_ms;             // Time to maintain lock (default: 2000ms)
    uint32_t cooldown_time_ms;         // Cooldown between detections (default: 500ms)
    bool audio_feedback;               // Enable audio target lock feedback
    bool visual_feedback;              // Enable LED visual feedback
    bool web_interface_updates;        // Send updates to web interface
    bool safety_mode;                  // Enable safety restrictions
} ai_targeting_config_t;

// Target information
typedef struct {
    target_status_t status;            // Current target status
    float confidence;                  // Detection confidence (0.0-1.0)
    uint32_t lock_duration_ms;         // How long target has been locked
    uint32_t detection_count;          // Number of consecutive detections
    uint32_t timestamp_ms;             // Last detection timestamp
    bool is_stable;                    // True if target is stable/consistent
} target_info_t;

// AI targeting statistics
typedef struct {
    uint32_t total_detections;         // Total detection attempts
    uint32_t successful_detections;    // Successful person detections
    uint32_t target_locks;             // Number of target locks achieved
    uint32_t false_positives;          // Estimated false positive count
    uint32_t avg_detection_time_ms;    // Average detection processing time
    float detection_accuracy;          // Detection accuracy percentage
} ai_targeting_stats_t;

// Callback function types
typedef void (*target_lock_callback_t)(const target_info_t* target, void* user_data);
typedef void (*target_lost_callback_t)(const target_info_t* target, void* user_data);

/**
 * @brief Initialize AI targeting system
 * 
 * Sets up integration between person detection, camera system, and weapon controls.
 * Must be called after camera and person detection systems are initialized.
 * 
 * @param config Pointer to AI targeting configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ai_targeting_init(const ai_targeting_config_t* config);

/**
 * @brief Deinitialize AI targeting system
 * 
 * @return ESP_OK on success
 */
esp_err_t ai_targeting_deinit(void);

/**
 * @brief Start AI targeting with specified mode
 * 
 * @param mode Targeting mode to activate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ai_targeting_start(ai_targeting_mode_t mode);

/**
 * @brief Stop AI targeting
 * 
 * @return ESP_OK on success
 */
esp_err_t ai_targeting_stop(void);

/**
 * @brief Process camera frame for AI targeting
 * 
 * Main processing function that takes camera frame, runs person detection,
 * and updates targeting status. Should be called for each camera frame.
 * 
 * @param frame_buffer Camera frame buffer
 * @param frame_width Frame width
 * @param frame_height Frame height
 * @param pixel_format Pixel format
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ai_targeting_process_frame(const uint8_t* frame_buffer,
                                   int frame_width,
                                   int frame_height,
                                   int pixel_format);

/**
 * @brief Get current target information
 * 
 * @param target_info Pointer to store current target information
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no target
 */
esp_err_t ai_targeting_get_target_info(target_info_t* target_info);

/**
 * @brief Check if target is currently locked
 * 
 * @return true if target is locked, false otherwise
 */
bool ai_targeting_is_target_locked(void);

/**
 * @brief Register target lock callback
 * 
 * @param callback Function to call when target is locked
 * @param user_data User data to pass to callback
 * @return ESP_OK on success
 */
esp_err_t ai_targeting_register_lock_callback(target_lock_callback_t callback, void* user_data);

/**
 * @brief Register target lost callback
 * 
 * @param callback Function to call when target is lost
 * @param user_data User data to pass to callback
 * @return ESP_OK on success
 */
esp_err_t ai_targeting_register_lost_callback(target_lost_callback_t callback, void* user_data);

/**
 * @brief Update AI targeting configuration
 * 
 * @param config New configuration to apply
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ai_targeting_update_config(const ai_targeting_config_t* config);

/**
 * @brief Get current AI targeting configuration
 * 
 * @param config Pointer to store current configuration
 * @return ESP_OK on success
 */
esp_err_t ai_targeting_get_config(ai_targeting_config_t* config);

/**
 * @brief Get AI targeting statistics
 * 
 * @param stats Pointer to store statistics
 * @return ESP_OK on success
 */
esp_err_t ai_targeting_get_stats(ai_targeting_stats_t* stats);

/**
 * @brief Reset AI targeting statistics
 * 
 * @return ESP_OK on success
 */
esp_err_t ai_targeting_reset_stats(void);

/**
 * @brief Manually trigger target acquisition
 * 
 * Forces a single detection cycle, useful for testing or manual targeting.
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ai_targeting_manual_trigger(void);

/**
 * @brief Enable/disable safety mode
 * 
 * Safety mode adds additional restrictions and confirmations before
 * allowing weapon activation with AI targeting.
 * 
 * @param enable true to enable safety mode, false to disable
 * @return ESP_OK on success
 */
esp_err_t ai_targeting_set_safety_mode(bool enable);

/**
 * @brief Check if AI targeting system is ready
 * 
 * @return true if system is initialized and ready, false otherwise
 */
bool ai_targeting_is_ready(void);

/**
 * @brief Get current AI targeting mode
 * 
 * @return Current targeting mode
 */
ai_targeting_mode_t ai_targeting_get_mode(void);

/**
 * @brief Get default AI targeting configuration
 * 
 * @return Default configuration structure
 */
ai_targeting_config_t ai_targeting_get_default_config(void);

// Web interface integration functions

/**
 * @brief Get AI targeting status for web interface
 * 
 * Returns JSON-formatted string with current AI targeting status,
 * suitable for web interface updates.
 * 
 * @param json_buffer Buffer to store JSON string
 * @param buffer_size Size of buffer
 * @return ESP_OK on success, ESP_ERR_INVALID_SIZE if buffer too small
 */
esp_err_t ai_targeting_get_web_status(char* json_buffer, size_t buffer_size);

/**
 * @brief Process web interface command
 * 
 * Processes AI targeting commands received from web interface.
 * 
 * @param command Command string from web interface
 * @param response_buffer Buffer to store response
 * @param response_size Size of response buffer
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ai_targeting_process_web_command(const char* command, 
                                         char* response_buffer, 
                                         size_t response_size);

// Integration with existing F44AA systems

/**
 * @brief Integration with magazine system
 * 
 * Links AI targeting with magazine status for smart ammo management.
 * 
 * @param rounds_remaining Current rounds in magazine
 * @return ESP_OK on success
 */
esp_err_t ai_targeting_update_ammo_status(int rounds_remaining);

/**
 * @brief Integration with trigger system
 * 
 * Provides AI targeting feedback to trigger system for smart firing control.
 * 
 * @param trigger_pulled true if trigger is currently pulled
 * @return ESP_OK on success
 */
esp_err_t ai_targeting_update_trigger_status(bool trigger_pulled);

/**
 * @brief Integration with LED system
 * 
 * Controls LED indicators based on AI targeting status.
 * 
 * @return ESP_OK on success
 */
esp_err_t ai_targeting_update_led_indicators(void);

#ifdef __cplusplus
}
#endif

#endif // AI_TARGETING_H
