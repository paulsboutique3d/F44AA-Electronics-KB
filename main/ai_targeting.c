/**
 * @file ai_targeting.c
 * @brief AI-Powered Targeting System Implementation
 * 
 * Implementation of intelligent targeting system that integrates ESP-DL
 * person detection with the F44AA weapon systems.
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

#include <inttypes.h>
#include "ai_targeting.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "AI_TARGETING";

// Global state structure
typedef struct {
    // Configuration and status
    ai_targeting_config_t config;
    ai_targeting_mode_t current_mode;
    target_info_t current_target;
    ai_targeting_stats_t stats;
    
    // State management
    bool initialized;
    bool running;
    SemaphoreHandle_t state_mutex;
    
    // Targeting state
    uint32_t last_detection_time;
    uint32_t target_lock_start_time;
    uint32_t consecutive_detections;
    float detection_history[5];  // Rolling average of recent detections
    uint8_t history_index;
    
    // Callbacks
    target_lock_callback_t lock_callback;
    void* lock_callback_data;
    target_lost_callback_t lost_callback;
    void* lost_callback_data;
    
    // Integration state
    int current_ammo;
    bool trigger_active;
    bool safety_enabled;
    
} ai_targeting_state_t;

static ai_targeting_state_t g_ai_state = {0};

// Forward declarations
static void update_target_status(const person_detection_result_t* detection);
static void process_target_lock(void);
static void process_target_lost(void);
static float calculate_detection_stability(void);
static esp_err_t validate_targeting_safety(void);

ai_targeting_config_t ai_targeting_get_default_config(void) {
    ai_targeting_config_t config = {
        .mode = AI_TARGETING_MONITOR,
        .detection_threshold = 0.7f,
        .lock_time_ms = 2000,
        .cooldown_time_ms = 500,
        .audio_feedback = true,
        .visual_feedback = true,
        .web_interface_updates = true,
        .safety_mode = true
    };
    return config;
}

esp_err_t ai_targeting_init(const ai_targeting_config_t* config) {
    if (g_ai_state.initialized) {
        ESP_LOGW(TAG, "AI targeting already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing AI targeting system");
    
    // Initialize configuration
    if (config) {
        g_ai_state.config = *config;
    } else {
        g_ai_state.config = ai_targeting_get_default_config();
    }
    
    // Create mutex for thread safety
    g_ai_state.state_mutex = xSemaphoreCreateMutex();
    if (!g_ai_state.state_mutex) {
        ESP_LOGE(TAG, "Failed to create state mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize state
    g_ai_state.current_mode = AI_TARGETING_OFF;
    g_ai_state.current_target.status = TARGET_STATUS_NONE;
    g_ai_state.running = false;
    g_ai_state.safety_enabled = g_ai_state.config.safety_mode;
    
    // Initialize statistics
    memset(&g_ai_state.stats, 0, sizeof(ai_targeting_stats_t));
    
    // Initialize detection history
    memset(g_ai_state.detection_history, 0, sizeof(g_ai_state.detection_history));
    g_ai_state.history_index = 0;
    
    // Initialize person detection system with simplified API
    esp_err_t ret = person_detection_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize person detection: %s", esp_err_to_name(ret));
        vSemaphoreDelete(g_ai_state.state_mutex);
        return ret;
    }
    
    g_ai_state.initialized = true;
    
    ESP_LOGI(TAG, "AI targeting initialized successfully");
    ESP_LOGI(TAG, "Mode: %d, Threshold: %.2f, Safety: %s", 
             g_ai_state.config.mode, 
             g_ai_state.config.detection_threshold,
             g_ai_state.safety_enabled ? "ON" : "OFF");
    
    return ESP_OK;
}

esp_err_t ai_targeting_start(ai_targeting_mode_t mode) {
    if (!g_ai_state.initialized) {
        ESP_LOGE(TAG, "AI targeting not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_ai_state.state_mutex, portMAX_DELAY);
    
    g_ai_state.current_mode = mode;
    g_ai_state.running = (mode != AI_TARGETING_OFF);
    
    if (g_ai_state.running) {
        // Reset targeting state
        g_ai_state.current_target.status = TARGET_STATUS_NONE;
        g_ai_state.consecutive_detections = 0;
        g_ai_state.last_detection_time = 0;
        g_ai_state.target_lock_start_time = 0;
        
        // Reset detection history
        memset(g_ai_state.detection_history, 0, sizeof(g_ai_state.detection_history));
        g_ai_state.history_index = 0;
        
        ESP_LOGI(TAG, "AI targeting started in mode: %d", mode);
    } else {
        ESP_LOGI(TAG, "AI targeting stopped");
    }
    
    xSemaphoreGive(g_ai_state.state_mutex);
    
    return ESP_OK;
}

esp_err_t ai_targeting_stop(void) {
    if (!g_ai_state.initialized) {
        return ESP_OK;
    }
    
    xSemaphoreTake(g_ai_state.state_mutex, portMAX_DELAY);
    
    g_ai_state.running = false;
    g_ai_state.current_mode = AI_TARGETING_OFF;
    g_ai_state.current_target.status = TARGET_STATUS_NONE;
    
    xSemaphoreGive(g_ai_state.state_mutex);
    
    ESP_LOGI(TAG, "AI targeting stopped");
    return ESP_OK;
}

esp_err_t ai_targeting_process_frame(const uint8_t* frame_buffer,
                                   int frame_width,
                                   int frame_height,
                                   int pixel_format) {
    if (!g_ai_state.initialized || !g_ai_state.running) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!frame_buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check cooldown period
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (g_ai_state.last_detection_time > 0 && 
        (current_time - g_ai_state.last_detection_time) < g_ai_state.config.cooldown_time_ms) {
        return ESP_OK;  // Still in cooldown
    }
    
    xSemaphoreTake(g_ai_state.state_mutex, portMAX_DELAY);
    
    // Perform person detection on frame
    person_detection_result_t detection_result;
    // Calculate image size and call inference directly  
    size_t image_size = frame_width * frame_height * 2; // Assuming RGB565
    esp_err_t ret = person_detection_run_inference(frame_buffer, image_size, &detection_result);
    
    if (ret == ESP_OK) {
        // Update statistics
        g_ai_state.stats.total_detections++;
        g_ai_state.stats.avg_detection_time_ms = 
            (g_ai_state.stats.avg_detection_time_ms + detection_result.inference_time_ms) / 2;
        
        if (detection_result.person_detected) {
            g_ai_state.stats.successful_detections++;
        }
        
        // Update detection accuracy
        if (g_ai_state.stats.total_detections > 0) {
            g_ai_state.stats.detection_accuracy = 
                (float)g_ai_state.stats.successful_detections / g_ai_state.stats.total_detections * 100.0f;
        }
        
        // Update target status based on detection
        update_target_status(&detection_result);
        
        // Update detection history for stability calculation
        g_ai_state.detection_history[g_ai_state.history_index] = detection_result.confidence;
        g_ai_state.history_index = (g_ai_state.history_index + 1) % 5;
        
        g_ai_state.last_detection_time = current_time;
        
        // Process targeting logic based on mode
        if (g_ai_state.current_mode != AI_TARGETING_MONITOR) {
            if (g_ai_state.current_target.status == TARGET_STATUS_DETECTED ||
                g_ai_state.current_target.status == TARGET_STATUS_LOCKED) {
                process_target_lock();
            } else if (g_ai_state.current_target.status == TARGET_STATUS_LOST) {
                process_target_lost();
            }
        }
    }
    
    xSemaphoreGive(g_ai_state.state_mutex);
    
    return ret;
}

static void update_target_status(const person_detection_result_t* detection) {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    if (detection->person_detected) {
        g_ai_state.consecutive_detections++;
        
        // Update target information
        g_ai_state.current_target.confidence = detection->confidence;
        g_ai_state.current_target.detection_count = g_ai_state.consecutive_detections;
        g_ai_state.current_target.timestamp_ms = current_time;
        g_ai_state.current_target.is_stable = calculate_detection_stability() > 0.8f;
        
        // Determine target status
        if (g_ai_state.current_target.status == TARGET_STATUS_NONE ||
            g_ai_state.current_target.status == TARGET_STATUS_LOST) {
            g_ai_state.current_target.status = TARGET_STATUS_DETECTED;
            ESP_LOGI(TAG, "Target detected (confidence: %.2f)", detection->confidence);
        } else if (g_ai_state.current_target.status == TARGET_STATUS_DETECTED &&
                   g_ai_state.consecutive_detections >= 3 &&
                   g_ai_state.current_target.is_stable) {
            g_ai_state.current_target.status = TARGET_STATUS_LOCKED;
            g_ai_state.target_lock_start_time = current_time;
            g_ai_state.stats.target_locks++;
            ESP_LOGI(TAG, "Target LOCKED (confidence: %.2f)", detection->confidence);
        }
        
        // Update lock duration
        if (g_ai_state.current_target.status == TARGET_STATUS_LOCKED) {
            g_ai_state.current_target.lock_duration_ms = 
                current_time - g_ai_state.target_lock_start_time;
        }
        
    } else {
        // No person detected
        g_ai_state.consecutive_detections = 0;
        
        if (g_ai_state.current_target.status == TARGET_STATUS_DETECTED ||
            g_ai_state.current_target.status == TARGET_STATUS_LOCKED) {
            g_ai_state.current_target.status = TARGET_STATUS_LOST;
            g_ai_state.current_target.timestamp_ms = current_time;
            ESP_LOGW(TAG, "Target lost");
        } else if (g_ai_state.current_target.status == TARGET_STATUS_LOST &&
                   (current_time - g_ai_state.current_target.timestamp_ms) > 1000) {
            // Target has been lost for more than 1 second
            g_ai_state.current_target.status = TARGET_STATUS_NONE;
            g_ai_state.current_target.lock_duration_ms = 0;
        }
    }
}

static void process_target_lock(void) {
    if (g_ai_state.current_target.status == TARGET_STATUS_LOCKED) {
        // Validate safety conditions
        esp_err_t safety_check = validate_targeting_safety();
        if (safety_check != ESP_OK) {
            ESP_LOGW(TAG, "Safety check failed, target lock disabled");
            g_ai_state.current_target.status = TARGET_STATUS_DETECTED;
            return;
        }
        
        // Call lock callback if registered
        if (g_ai_state.lock_callback) {
            g_ai_state.lock_callback(&g_ai_state.current_target, g_ai_state.lock_callback_data);
        }
        
        // Update LED indicators if enabled
        if (g_ai_state.config.visual_feedback) {
            ai_targeting_update_led_indicators();
        }
        
        // Check if target lock should expire
        if (g_ai_state.current_target.lock_duration_ms > g_ai_state.config.lock_time_ms) {
            ESP_LOGI(TAG, "Target lock expired");
            g_ai_state.current_target.status = TARGET_STATUS_DETECTED;
        }
    }
}

static void process_target_lost(void) {
    // Call lost callback if registered
    if (g_ai_state.lost_callback) {
        g_ai_state.lost_callback(&g_ai_state.current_target, g_ai_state.lost_callback_data);
    }
    
    ESP_LOGI(TAG, "Processing target lost");
}

static float calculate_detection_stability(void) {
    float sum = 0.0f;
    int count = 0;
    
    for (int i = 0; i < 5; i++) {
        if (g_ai_state.detection_history[i] > 0.0f) {
            sum += g_ai_state.detection_history[i];
            count++;
        }
    }
    
    if (count == 0) return 0.0f;
    
    float average = sum / count;
    float variance = 0.0f;
    
    for (int i = 0; i < 5; i++) {
        if (g_ai_state.detection_history[i] > 0.0f) {
            float diff = g_ai_state.detection_history[i] - average;
            variance += diff * diff;
        }
    }
    
    variance /= count;
    float stability = 1.0f - (variance / (average * average));
    
    return stability > 0.0f ? stability : 0.0f;
}

static esp_err_t validate_targeting_safety(void) {
    // Safety check 1: Safety mode enabled
    if (g_ai_state.safety_enabled) {
        // Additional safety restrictions in safety mode
        if (g_ai_state.current_target.confidence < 0.8f) {
            return ESP_ERR_INVALID_STATE;
        }
        
        if (g_ai_state.consecutive_detections < 5) {
            return ESP_ERR_INVALID_STATE;
        }
    }
    
    // Safety check 2: Minimum lock duration for auto mode
    if (g_ai_state.current_mode == AI_TARGETING_AUTO &&
        g_ai_state.current_target.lock_duration_ms < 1000) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Safety check 3: Check ammo availability
    if (g_ai_state.current_ammo <= 0) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return ESP_OK;
}

esp_err_t ai_targeting_get_target_info(target_info_t* target_info) {
    if (!target_info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_ai_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_ai_state.state_mutex, portMAX_DELAY);
    *target_info = g_ai_state.current_target;
    xSemaphoreGive(g_ai_state.state_mutex);
    
    return g_ai_state.current_target.status != TARGET_STATUS_NONE ? ESP_OK : ESP_ERR_NOT_FOUND;
}

bool ai_targeting_is_target_locked(void) {
    if (!g_ai_state.initialized) {
        return false;
    }
    
    return g_ai_state.current_target.status == TARGET_STATUS_LOCKED;
}

esp_err_t ai_targeting_register_lock_callback(target_lock_callback_t callback, void* user_data) {
    if (!g_ai_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_ai_state.state_mutex, portMAX_DELAY);
    g_ai_state.lock_callback = callback;
    g_ai_state.lock_callback_data = user_data;
    xSemaphoreGive(g_ai_state.state_mutex);
    
    return ESP_OK;
}

esp_err_t ai_targeting_register_lost_callback(target_lost_callback_t callback, void* user_data) {
    if (!g_ai_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_ai_state.state_mutex, portMAX_DELAY);
    g_ai_state.lost_callback = callback;
    g_ai_state.lost_callback_data = user_data;
    xSemaphoreGive(g_ai_state.state_mutex);
    
    return ESP_OK;
}

esp_err_t ai_targeting_get_stats(ai_targeting_stats_t* stats) {
    if (!stats || !g_ai_state.initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_ai_state.state_mutex, portMAX_DELAY);
    *stats = g_ai_state.stats;
    xSemaphoreGive(g_ai_state.state_mutex);
    
    return ESP_OK;
}

esp_err_t ai_targeting_set_safety_mode(bool enable) {
    if (!g_ai_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_ai_state.state_mutex, portMAX_DELAY);
    g_ai_state.safety_enabled = enable;
    xSemaphoreGive(g_ai_state.state_mutex);
    
    ESP_LOGI(TAG, "Safety mode %s", enable ? "ENABLED" : "DISABLED");
    return ESP_OK;
}

bool ai_targeting_is_ready(void) {
    return g_ai_state.initialized && person_detection_is_running();
}

ai_targeting_mode_t ai_targeting_get_mode(void) {
    return g_ai_state.current_mode;
}

esp_err_t ai_targeting_get_web_status(char* json_buffer, size_t buffer_size) {
    if (!json_buffer || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_ai_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_ai_state.state_mutex, portMAX_DELAY);
    
    int written = snprintf(json_buffer, buffer_size,
        "{"
        "\"mode\":%d,"
        "\"running\":%s,"
        "\"target_status\":%d,"
        "\"target_locked\":%s,"
        "\"confidence\":%.2f,"
        "\"detection_count\":%" PRIu32 ","
        "\"total_detections\":%" PRIu32 ","
        "\"target_locks\":%" PRIu32 ","
        "\"accuracy\":%.1f,"
        "\"safety_enabled\":%s"
        "}",
        g_ai_state.current_mode,
        g_ai_state.running ? "true" : "false",
        g_ai_state.current_target.status,
        (g_ai_state.current_target.status == TARGET_STATUS_LOCKED) ? "true" : "false",
        g_ai_state.current_target.confidence,
        g_ai_state.current_target.detection_count,
        g_ai_state.stats.total_detections,
        g_ai_state.stats.target_locks,
        g_ai_state.stats.detection_accuracy,
        g_ai_state.safety_enabled ? "true" : "false"
    );
    
    xSemaphoreGive(g_ai_state.state_mutex);
    
    return (written < buffer_size) ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

esp_err_t ai_targeting_update_ammo_status(int rounds_remaining) {
    if (!g_ai_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_ai_state.state_mutex, portMAX_DELAY);
    g_ai_state.current_ammo = rounds_remaining;
    xSemaphoreGive(g_ai_state.state_mutex);
    
    return ESP_OK;
}

esp_err_t ai_targeting_update_trigger_status(bool trigger_pulled) {
    if (!g_ai_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_ai_state.state_mutex, portMAX_DELAY);
    g_ai_state.trigger_active = trigger_pulled;
    xSemaphoreGive(g_ai_state.state_mutex);
    
    return ESP_OK;
}

esp_err_t ai_targeting_update_led_indicators(void) {
    // This function would integrate with your existing LED system
    // to provide visual feedback based on AI targeting status
    
    if (!g_ai_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Implementation would depend on your existing LED control system
    // For now, just log the status
    switch (g_ai_state.current_target.status) {
        case TARGET_STATUS_DETECTED:
            ESP_LOGI(TAG, "LED: Target detected indicator");
            break;
        case TARGET_STATUS_LOCKED:
            ESP_LOGI(TAG, "LED: Target locked indicator");
            break;
        case TARGET_STATUS_LOST:
            ESP_LOGI(TAG, "LED: Target lost indicator");
            break;
        default:
            ESP_LOGI(TAG, "LED: No target indicator");
            break;
    }
    
    return ESP_OK;
}

esp_err_t ai_targeting_deinit(void) {
    if (!g_ai_state.initialized) {
        return ESP_OK;
    }
    
    // Stop targeting if running
    ai_targeting_stop();
    
    // Cleanup person detection
    person_detection_deinit();
    
    // Cleanup mutex
    if (g_ai_state.state_mutex) {
        vSemaphoreDelete(g_ai_state.state_mutex);
        g_ai_state.state_mutex = NULL;
    }
    
    // Reset state
    memset(&g_ai_state, 0, sizeof(ai_targeting_state_t));
    
    ESP_LOGI(TAG, "AI targeting deinitialized");
    return ESP_OK;
}
