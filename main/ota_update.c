/**
 * @file ota_update.c
 * @brief Over-The-Air Firmware Update Handler
 * 
 * Manages OTA firmware updates via web interface. Supports chunked
 * upload with progress tracking and automatic reboot on completion.
 * 
 * Features:
 * - Chunked binary upload handling
 * - Progress tracking and status reporting
 * - Automatic partition switching
 * - Error handling and rollback capability
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 * 
 * Partition: Uses ESP-IDF OTA partition scheme
 */

#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include <string.h>
#include "ota_update.h"

static const char *TAG = "OTA_UPDATE";

static esp_ota_handle_t ota_handle = 0;
static const esp_partition_t *ota_partition = NULL;
static bool ota_in_progress = false;
static size_t ota_received_bytes = 0;
static size_t ota_total_bytes = 0;

// OTA Status tracking
static ota_status_t current_ota_status = {
    .state = OTA_STATE_IDLE,
    .progress = 0,
    .error_message = ""
};

esp_err_t ota_begin_update(size_t content_length) {
    if (ota_in_progress) {
        ESP_LOGE(TAG, "OTA update already in progress");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Starting OTA update, size: %d bytes", content_length);
    
    // Get the next OTA partition
    ota_partition = esp_ota_get_next_update_partition(NULL);
    if (ota_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        current_ota_status.state = OTA_STATE_ERROR;
        strcpy(current_ota_status.error_message, "No OTA partition found");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA partition found: %s at offset 0x%lx", 
             ota_partition->label, (unsigned long)ota_partition->address);

    // Begin OTA update
    esp_err_t err = esp_ota_begin(ota_partition, content_length, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        current_ota_status.state = OTA_STATE_ERROR;
        snprintf(current_ota_status.error_message, sizeof(current_ota_status.error_message), 
                "OTA begin failed: %s", esp_err_to_name(err));
        return err;
    }

    ota_in_progress = true;
    ota_received_bytes = 0;
    ota_total_bytes = content_length;
    current_ota_status.state = OTA_STATE_DOWNLOADING;
    current_ota_status.progress = 0;
    strcpy(current_ota_status.error_message, "");

    ESP_LOGI(TAG, "OTA update started successfully");
    return ESP_OK;
}

esp_err_t ota_write_data(const void *data, size_t size) {
    if (!ota_in_progress) {
        ESP_LOGE(TAG, "OTA not in progress");
        return ESP_FAIL;
    }

    esp_err_t err = esp_ota_write(ota_handle, data, size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
        current_ota_status.state = OTA_STATE_ERROR;
        snprintf(current_ota_status.error_message, sizeof(current_ota_status.error_message), 
                "OTA write failed: %s", esp_err_to_name(err));
        return err;
    }

    ota_received_bytes += size;
    
    // Update progress
    if (ota_total_bytes > 0) {
        current_ota_status.progress = (ota_received_bytes * 100) / ota_total_bytes;
    }

    ESP_LOGD(TAG, "OTA progress: %d%% (%d/%d bytes)", 
             current_ota_status.progress, ota_received_bytes, ota_total_bytes);

    return ESP_OK;
}

esp_err_t ota_end_update(void) {
    if (!ota_in_progress) {
        ESP_LOGE(TAG, "OTA not in progress");
        return ESP_FAIL;
    }

    esp_err_t err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        current_ota_status.state = OTA_STATE_ERROR;
        snprintf(current_ota_status.error_message, sizeof(current_ota_status.error_message), 
                "OTA end failed: %s", esp_err_to_name(err));
        ota_in_progress = false;
        return err;
    }

    // Set the new partition as boot partition
    err = esp_ota_set_boot_partition(ota_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        current_ota_status.state = OTA_STATE_ERROR;
        snprintf(current_ota_status.error_message, sizeof(current_ota_status.error_message), 
                "Set boot partition failed: %s", esp_err_to_name(err));
        ota_in_progress = false;
        return err;
    }

    ota_in_progress = false;
    current_ota_status.state = OTA_STATE_COMPLETE;
    current_ota_status.progress = 100;
    strcpy(current_ota_status.error_message, "");

    ESP_LOGI(TAG, "OTA update completed successfully!");
    ESP_LOGI(TAG, "Device will restart in 3 seconds...");

    return ESP_OK;
}

void ota_abort_update(void) {
    if (ota_in_progress) {
        esp_ota_abort(ota_handle);
        ota_in_progress = false;
        current_ota_status.state = OTA_STATE_ERROR;
        strcpy(current_ota_status.error_message, "OTA update aborted");
        ESP_LOGW(TAG, "OTA update aborted");
    }
}

ota_status_t ota_get_status(void) {
    return current_ota_status;
}

bool ota_is_in_progress(void) {
    return ota_in_progress;
}

void ota_restart_device(void) {
    ESP_LOGI(TAG, "Restarting device...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

esp_err_t ota_get_current_version(char *version_buffer, size_t buffer_size) {
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();
    if (app_desc == NULL) {
        return ESP_FAIL;
    }
    
    snprintf(version_buffer, buffer_size, "%s", app_desc->version);
    return ESP_OK;
}

esp_err_t ota_get_partition_info(char *info_buffer, size_t buffer_size) {
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
    
    if (running_partition == NULL || boot_partition == NULL) {
        return ESP_FAIL;
    }
    
    snprintf(info_buffer, buffer_size, 
             "Running: %s (0x%lx), Boot: %s (0x%lx)", 
             running_partition->label, (unsigned long)running_partition->address,
             boot_partition->label, (unsigned long)boot_partition->address);
    
    return ESP_OK;
}
