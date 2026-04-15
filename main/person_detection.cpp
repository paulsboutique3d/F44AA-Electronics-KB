/**
 * @file person_detection.cpp
 * @brief ESP-DL Person Detection Implementation
 * 
 * Real-time person detection using ESP-DL pedestrian detection model.
 * Processes camera frames and provides bounding box coordinates for
 * detected persons.
 * 
 * Features:
 * - ESP-DL pedestrian detection model
 * - Configurable confidence threshold
 * - Performance statistics tracking
 * - C wrapper functions for integration
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 * 
 * Model: espressif/pedestrian_detect
 * Performance: ~50-80ms inference on ESP32-S3
 */

#include "person_detection.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <new>

// ESP-DL includes
#include "pedestrian_detect.hpp"

static const char *TAG = "person_detection";

// Global state
static PedestrianDetect *detector = nullptr;
static bool is_initialized = false;
static bool is_running = false;
static float confidence_threshold = PERSON_DETECTION_DEFAULT_CONFIDENCE;
static person_detection_performance_t performance_stats = {
    .total_inferences = 0,
    .total_detections = 0,
    .avg_inference_time_ms = 0,
    .last_inference_time_ms = 0,
    .fps = 0.0f
};
static person_detection_result_t last_result = {
    .person_detected = false,
    .confidence = 0.0f,
    .bbox = {0, 0, 0, 0},
    .inference_time_ms = 0,
    .timestamp = 0
};

// Forward declarations for C++ wrapper functions
extern "C" {
    esp_err_t cpp_init_pedestrian_detector(void);
    void cpp_cleanup_pedestrian_detector(void);
    bool cpp_run_pedestrian_detection(const uint8_t* image_data, int width, int height, person_detection_result_t* result);
}

esp_err_t person_detection_init(void)
{
    if (is_initialized) {
        ESP_LOGW(TAG, "Person detection already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing ESP-DL pedestrian detection...");
    
    esp_err_t ret = cpp_init_pedestrian_detector();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ESP-DL detector");
        return ret;
    }
    
    // Reset statistics
    memset(&performance_stats, 0, sizeof(performance_stats));
    memset(&last_result, 0, sizeof(last_result));
    
    is_initialized = true;
    ESP_LOGI(TAG, "Person detection initialized successfully");
    return ESP_OK;
}

esp_err_t person_detection_deinit(void)
{
    if (!is_initialized) {
        return ESP_OK;
    }
    
    if (is_running) {
        person_detection_stop();
    }
    
    cpp_cleanup_pedestrian_detector();
    is_initialized = false;
    ESP_LOGI(TAG, "Person detection deinitialized");
    return ESP_OK;
}

esp_err_t person_detection_start(void)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "Person detection not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    is_running = true;
    ESP_LOGI(TAG, "Person detection started");
    return ESP_OK;
}

esp_err_t person_detection_stop(void)
{
    is_running = false;
    ESP_LOGI(TAG, "Person detection stopped");
    return ESP_OK;
}

esp_err_t person_detection_run_inference(const uint8_t* image_data, size_t image_size, 
                                        person_detection_result_t* result)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "Person detection not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!image_data || !result) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    int64_t start_time = esp_timer_get_time();
    
    // Assume RGB565 format with typical ESP32-CAM resolution
    // For now, use fixed dimensions - this would need proper image parsing in real implementation
    int width = 320;   // Typical ESP32-CAM width
    int height = 240;  // Typical ESP32-CAM height
    
    bool detected = cpp_run_pedestrian_detection(image_data, width, height, result);
    
    int64_t inference_time = esp_timer_get_time() - start_time;
    result->inference_time_ms = inference_time / 1000;
    result->timestamp = esp_timer_get_time() / 1000;
    
    // Update performance statistics
    performance_stats.total_inferences++;
    if (detected) {
        performance_stats.total_detections++;
    }
    
    performance_stats.last_inference_time_ms = result->inference_time_ms;
    
    // Update rolling average
    if (performance_stats.total_inferences == 1) {
        performance_stats.avg_inference_time_ms = result->inference_time_ms;
    } else {
        performance_stats.avg_inference_time_ms = 
            (performance_stats.avg_inference_time_ms * (performance_stats.total_inferences - 1) + 
             result->inference_time_ms) / performance_stats.total_inferences;
    }
    
    // Calculate FPS based on recent performance
    if (result->inference_time_ms > 0) {
        performance_stats.fps = 1000.0f / result->inference_time_ms;
    }
    
    // Store last result
    memcpy(&last_result, result, sizeof(person_detection_result_t));
    
    if (detected) {
        ESP_LOGI(TAG, "Person detected! Confidence: %.2f, Time: %u ms", 
                 result->confidence, result->inference_time_ms);
    }
    
    return ESP_OK;
}

esp_err_t person_detection_set_confidence_threshold(float threshold)
{
    if (threshold < 0.0f || threshold > 1.0f) {
        ESP_LOGE(TAG, "Invalid threshold: %.2f", threshold);
        return ESP_ERR_INVALID_ARG;
    }
    
    confidence_threshold = threshold;
    ESP_LOGI(TAG, "Confidence threshold set to %.2f", threshold);
    return ESP_OK;
}

float person_detection_get_confidence_threshold(void)
{
    return confidence_threshold;
}

person_detection_state_t person_detection_get_state(void)
{
    person_detection_state_t state;
    state.is_initialized = is_initialized;
    state.is_running = is_running;
    state.confidence_threshold = confidence_threshold;
    state.last_detection_time = last_result.timestamp;
    state.detection_count = performance_stats.total_detections;
    return state;
}

person_detection_model_info_t person_detection_get_model_info(void)
{
    person_detection_model_info_t info;
    strncpy(info.model_name, "ESP-DL Pedestrian Detection PICO S8 V1", sizeof(info.model_name) - 1);
    info.model_name[sizeof(info.model_name) - 1] = '\0';
    
    info.input_width = 192;    // PICO model typical input size
    info.input_height = 192;
    info.input_channels = 3;   // RGB
    info.model_size = 350000;  // Approximate size in bytes
    info.quantization = QUANTIZATION_INT8;
    return info;
}

person_detection_performance_t person_detection_get_performance(void)
{
    return performance_stats;
}

bool person_detection_is_initialized(void)
{
    return is_initialized;
}

bool person_detection_is_running(void)
{
    return is_running;
}

esp_err_t person_detection_reset_stats(void)
{
    memset(&performance_stats, 0, sizeof(performance_stats));
    ESP_LOGI(TAG, "Performance statistics reset");
    return ESP_OK;
}

// C++ implementation functions
esp_err_t cpp_init_pedestrian_detector(void)
{
    detector = new(std::nothrow) PedestrianDetect(PedestrianDetect::PICO_S8_V1);
    if (!detector) {
        ESP_LOGE(TAG, "Failed to allocate memory for pedestrian detector");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void cpp_cleanup_pedestrian_detector(void)
{
    if (detector) {
        delete detector;
        detector = nullptr;
    }
}

bool cpp_run_pedestrian_detection(const uint8_t* image_data, int width, int height, person_detection_result_t* result)
{
    if (!detector) {
        return false;
    }
    
    // Create ESP-DL image structure
    dl::image::img_t img;
    img.data = (void*)image_data;
    img.width = width;
    img.height = height;
    img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565;  // Assume RGB565 from ESP32-CAM
    
    // Run inference
    std::list<dl::detect::result_t> &detection_results = detector->run(img);
    
    // Initialize result
    result->person_detected = false;
    result->confidence = 0.0f;
    result->bbox.x = 0;
    result->bbox.y = 0;
    result->bbox.width = 0;
    result->bbox.height = 0;
    
    // Find best detection above threshold
    float best_confidence = 0.0f;
    dl::detect::result_t best_detection;
    bool found_person = false;
    
    for (const auto& detection : detection_results) {
        // ESP-DL pedestrian detect typically returns category 0 for person
        if (detection.category == 0 && detection.score >= confidence_threshold) {
            if (detection.score > best_confidence) {
                best_confidence = detection.score;
                best_detection = detection;
                found_person = true;
            }
        }
    }
    
    if (found_person) {
        result->person_detected = true;
        result->confidence = best_detection.score;
        result->bbox.x = best_detection.box[0];
        result->bbox.y = best_detection.box[1];
        result->bbox.width = best_detection.box[2] - best_detection.box[0];
        result->bbox.height = best_detection.box[3] - best_detection.box[1];
        
        // Ensure bounding box is within image bounds
        if (result->bbox.x < 0) result->bbox.x = 0;
        if (result->bbox.y < 0) result->bbox.y = 0;
        if (result->bbox.x + result->bbox.width > width) {
            result->bbox.width = width - result->bbox.x;
        }
        if (result->bbox.y + result->bbox.height > height) {
            result->bbox.height = height - result->bbox.y;
        }
    }
    
    return found_person;
}
