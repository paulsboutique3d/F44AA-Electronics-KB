/**
 * @file person_detection.h
 * @brief AI Person Detection for F44AA Pulse Rifle
 * 
 * ESP-DL powered person detection for intelligent targeting.
 * 
 * Features:
 * - Real-time person detection
 * - ESP-NN optimized performance (~50-80ms on ESP32-S3)
 * - Integration with camera and web interface
 * - Configurable detection thresholds
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 */

#ifndef PERSON_DETECTION_H
#define PERSON_DETECTION_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configuration constants
#define PERSON_DETECTION_DEFAULT_CONFIDENCE 0.7f
#define PERSON_DETECTION_MAX_DETECTIONS 100

// Quantization types
typedef enum {
    QUANTIZATION_FLOAT32 = 0,
    QUANTIZATION_INT8 = 1
} quantization_type_t;

// Bounding box structure
typedef struct {
    int x;      // X coordinate
    int y;      // Y coordinate  
    int width;  // Width
    int height; // Height
} detection_bbox_t;

// Detection result structure
typedef struct {
    bool person_detected;           // True if person detected
    float confidence;               // Detection confidence (0.0-1.0)
    detection_bbox_t bbox;          // Bounding box of detected person
    uint32_t inference_time_ms;     // Time taken for inference
    uint64_t timestamp;             // Detection timestamp (ms)
} person_detection_result_t;

// Detection state structure
typedef struct {
    bool is_initialized;            // System initialization state
    bool is_running;                // Detection running state
    float confidence_threshold;     // Minimum confidence for detection
    uint64_t last_detection_time;   // Last successful detection time
    uint32_t detection_count;       // Total number of detections
} person_detection_state_t;

// Model information structure
typedef struct {
    char model_name[64];            // Model name
    int input_width;                // Input image width
    int input_height;               // Input image height
    int input_channels;             // Input image channels
    uint32_t model_size;            // Model size in bytes
    quantization_type_t quantization; // Model quantization type
} person_detection_model_info_t;

// Performance metrics structure
typedef struct {
    uint32_t total_inferences;      // Total number of inferences
    uint32_t total_detections;      // Total number of positive detections
    float avg_inference_time_ms;    // Average inference time
    uint32_t last_inference_time_ms; // Last inference time
    float fps;                      // Frames per second
} person_detection_performance_t;

/**
 * @brief Initialize person detection system
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t person_detection_init(void);

/**
 * @brief Deinitialize person detection system
 * 
 * @return ESP_OK on success
 */
esp_err_t person_detection_deinit(void);

/**
 * @brief Start person detection
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t person_detection_start(void);

/**
 * @brief Stop person detection
 * 
 * @return ESP_OK on success
 */
esp_err_t person_detection_stop(void);

/**
 * @brief Run inference on image data
 * 
 * @param image_data Pointer to image data
 * @param image_size Size of image data in bytes
 * @param result Pointer to store detection result
 * @return ESP_OK on success, error code on failure
 */
esp_err_t person_detection_run_inference(const uint8_t* image_data, size_t image_size, 
                                        person_detection_result_t* result);

/**
 * @brief Set confidence threshold for detection
 * 
 * @param threshold Confidence threshold (0.0-1.0)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t person_detection_set_confidence_threshold(float threshold);

/**
 * @brief Get current confidence threshold
 * 
 * @return Current confidence threshold
 */
float person_detection_get_confidence_threshold(void);

/**
 * @brief Get current detection state
 * 
 * @return Current detection state structure
 */
person_detection_state_t person_detection_get_state(void);

/**
 * @brief Get model information
 * 
 * @return Model information structure
 */
person_detection_model_info_t person_detection_get_model_info(void);

/**
 * @brief Get performance metrics
 * 
 * @return Performance metrics structure
 */
person_detection_performance_t person_detection_get_performance(void);

/**
 * @brief Check if system is initialized
 * 
 * @return True if initialized, false otherwise
 */
bool person_detection_is_initialized(void);

/**
 * @brief Check if detection is running
 * 
 * @return True if running, false otherwise
 */
bool person_detection_is_running(void);

/**
 * @brief Reset detection statistics
 * 
 * @return ESP_OK on success
 */
esp_err_t person_detection_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif // PERSON_DETECTION_H
