/**
 * @file ota_update.h
 * @brief Over-The-Air Update Interface
 * 
 * OTA firmware update handling via web interface.
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// OTA State enumeration
typedef enum {
    OTA_STATE_IDLE,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_COMPLETE,
    OTA_STATE_ERROR
} ota_state_t;

// OTA Status structure
typedef struct {
    ota_state_t state;
    int progress;
    char error_message[128];
} ota_status_t;

// OTA update functions
esp_err_t ota_begin_update(size_t content_length);
esp_err_t ota_write_data(const void *data, size_t size);
esp_err_t ota_end_update(void);
void ota_abort_update(void);

// OTA status functions
ota_status_t ota_get_status(void);
bool ota_is_in_progress(void);
void ota_restart_device(void);

// OTA information functions
esp_err_t ota_get_current_version(char *version_buffer, size_t buffer_size);
esp_err_t ota_get_partition_info(char *info_buffer, size_t buffer_size);