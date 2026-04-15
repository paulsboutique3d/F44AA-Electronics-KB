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
    float detection_threshold;     // Minimum confidence for person detection (default: 0.6)
    bool continuous_detection;     // Enable continuous detection mode
    uint32_t detection_interval_ms; // Interval between detections (default: 100ms)
    bool enable_esp_nn;           // Enable ESP-NN optimization (default: true)
    bool debug_output;            // Enable debug logging
} detection_config_t;

// Detection callback function type
typedef void (*detection_callback_t)(const detection_result_t* result, void* user_data);

// Detection modes
typedef enum {
    DETECTION_MODE_DISABLED = 0,    // Detection disabled
    DETECTION_MODE_SINGLE,          // Single shot detection
    DETECTION_MODE_CONTINUOUS,      // Continuous detection
    DETECTION_MODE_TRIGGERED        // Triggered by external event
} detection_mode_t;

// Detection status
typedef enum {
    DETECTION_STATUS_UNINITIALIZED = 0,
    DETECTION_STATUS_INITIALIZING,
    DETECTION_STATUS_READY,
    DETECTION_STATUS_RUNNING,
    DETECTION_STATUS_ERROR
} detection_status_t;

/**
 * @brief Initialize TensorFlow Lite Micro person detection system
 * 
 * Sets up the TensorFlow Lite interpreter, loads the person detection model,
 * allocates tensor arena memory, and initializes ESP-NN optimizations.
 * 
 * @param config Pointer to detection configuration structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t person_detection_init(const detection_config_t* config);

/**
 * @brief Deinitialize person detection system
 * 
 * Cleans up allocated memory and stops detection tasks.
 * 
 * @return ESP_OK on success
 */
esp_err_t person_detection_deinit(void);

/**
 * @brief Start person detection with specified mode
 * 
 * @param mode Detection mode to use
 * @return ESP_OK on success, error code on failure
 */
esp_err_t person_detection_start(detection_mode_t mode);

/**
 * @brief Stop person detection
 * 
 * @return ESP_OK on success
 */
esp_err_t person_detection_stop(void);

/**
 * @brief Perform single person detection on image data
 * 
 * Processes a single 96x96 grayscale image through the TensorFlow model
 * and returns detection results.
 * 
 * @param image_data Pointer to 96x96 grayscale image data (9216 bytes)
 * @param result Pointer to store detection result
 * @return ESP_OK on success, error code on failure
 */
esp_err_t person_detection_detect(const uint8_t* image_data, detection_result_t* result);

/**
 * @brief Detect person in camera frame buffer
 * 
 * Convenience function that takes a camera frame buffer, resizes/converts
 * to 96x96 grayscale, and performs detection.
 * 
 * @param frame_buffer Pointer to camera frame buffer
 * @param frame_width Width of input frame
 * @param frame_height Height of input frame
 * @param pixel_format Pixel format of input frame
 * @param result Pointer to store detection result
 * @return ESP_OK on success, error code on failure
 */
esp_err_t person_detection_detect_frame(const uint8_t* frame_buffer, 
                                       int frame_width, 
                                       int frame_height,
                                       int pixel_format,
                                       detection_result_t* result);

/**
 * @brief Register callback for detection results
 * 
 * @param callback Function to call when detection completes
 * @param user_data User data to pass to callback
 * @return ESP_OK on success
 */
esp_err_t person_detection_register_callback(detection_callback_t callback, void* user_data);

/**
 * @brief Get current detection status
 * 
 * @return Current detection system status
 */
detection_status_t person_detection_get_status(void);

/**
 * @brief Get detection statistics
 * 
 * @param total_detections Pointer to store total detection count
 * @param persons_detected Pointer to store person detection count
 * @param avg_inference_time_ms Pointer to store average inference time
 * @return ESP_OK on success
 */
esp_err_t person_detection_get_stats(uint32_t* total_detections, 
                                    uint32_t* persons_detected,
                                    uint32_t* avg_inference_time_ms);

/**
 * @brief Update detection configuration
 * 
 * @param config New configuration to apply
 * @return ESP_OK on success, error code on failure
 */
esp_err_t person_detection_update_config(const detection_config_t* config);

/**
 * @brief Get current detection configuration
 * 
 * @param config Pointer to store current configuration
 * @return ESP_OK on success
 */
esp_err_t person_detection_get_config(detection_config_t* config);

/**
 * @brief Trigger single detection (for triggered mode)
 * 
 * @return ESP_OK on success, error code if not in triggered mode
 */
esp_err_t person_detection_trigger(void);

/**
 * @brief Get last detection result
 * 
 * @param result Pointer to store last detection result
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no detection performed yet
 */
esp_err_t person_detection_get_last_result(detection_result_t* result);

/**
 * @brief Check if detection system is ready
 * 
 * @return true if ready for detection, false otherwise
 */
bool person_detection_is_ready(void);

/**
 * @brief Get default detection configuration
 * 
 * @return Default configuration structure
 */
detection_config_t person_detection_get_default_config(void);

// Utility functions for integration

/**
 * @brief Convert camera frame to detection input format
 * 
 * Converts and resizes camera frame to 96x96 grayscale format required
 * by the person detection model.
 * 
 * @param src_buffer Source frame buffer
 * @param src_width Source frame width
 * @param src_height Source frame height
 * @param src_format Source pixel format
 * @param dst_buffer Destination buffer (must be 9216 bytes)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t person_detection_convert_frame(const uint8_t* src_buffer,
                                        int src_width,
                                        int src_height, 
                                        int src_format,
                                        uint8_t* dst_buffer);

// Internal utility functions
static esp_err_t resize_grayscale_image(const uint8_t* src, int src_w, int src_h,
                                       uint8_t* dst, int dst_w, int dst_h);

static esp_err_t convert_rgb565_to_grayscale_and_resize(const uint8_t* src, int src_w, int src_h,
                                                       uint8_t* dst, int dst_w, int dst_h);

/**
 * @brief Get memory usage information
 * 
 * @param tensor_arena_used Bytes used in tensor arena
 * @param total_heap_used Total heap memory used by detection system
 * @return ESP_OK on success
 */
esp_err_t person_detection_get_memory_usage(size_t* tensor_arena_used, size_t* total_heap_used);

#ifdef __cplusplus
}
#endif

#endif // PERSON_DETECTION_H
