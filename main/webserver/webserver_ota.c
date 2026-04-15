#include "webserver_ota.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Define MIN macro if not already defined
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "WebServer_OTA";

esp_err_t webserver_ota_upload_handler(httpd_req_t *req) {
    char boundary[128];
    char content_type[256];
    
    if (ota_is_in_progress()) {
        WEBSERVER_SEND_ERROR(req, "409 Conflict", "OTA update already in progress");
    }
    
    // Get content type and boundary
    size_t content_type_len = httpd_req_get_hdr_value_len(req, "Content-Type") + 1;
    if (content_type_len > sizeof(content_type)) {
        WEBSERVER_SEND_ERROR(req, HTTP_400_BAD_REQUEST, "Content-Type header too long");
    }
    
    httpd_req_get_hdr_value_str(req, "Content-Type", content_type, content_type_len);
    
    // Extract boundary from Content-Type
    char *boundary_start = strstr(content_type, "boundary=");
    if (!boundary_start) {
        WEBSERVER_SEND_ERROR(req, HTTP_400_BAD_REQUEST, "Missing boundary in Content-Type");
    }
    
    strcpy(boundary, boundary_start + 9);
    
    ESP_LOGI(TAG, "Starting OTA upload, Content-Length: %zu", req->content_len);
    
    // Begin OTA update
    if (ota_begin_update(req->content_len) != ESP_OK) {
        WEBSERVER_SEND_ERROR(req, HTTP_500_INTERNAL_ERROR, "Failed to begin OTA update");
    }
    
    // Process multipart data
    char buffer[1024];
    int remaining = req->content_len;
    bool file_started = false;
    
    while (remaining > 0) {
        int received = httpd_req_recv(req, buffer, MIN(remaining, sizeof(buffer)));
        if (received <= 0) {
            ota_abort_update();
            WEBSERVER_SEND_ERROR(req, HTTP_400_BAD_REQUEST, "Failed to receive data");
        }
        
        if (!file_started) {
            // Look for the start of file data (after headers)
            char *file_start = strstr(buffer, "\r\n\r\n");
            if (file_start) {
                file_start += 4;
                int header_len = file_start - buffer;
                int data_len = received - header_len;
                
                if (data_len > 0) {
                    if (ota_write_data(file_start, data_len) != ESP_OK) {
                        ota_abort_update();
                        WEBSERVER_SEND_ERROR(req, HTTP_500_INTERNAL_ERROR, "OTA write failed");
                    }
                }
                file_started = true;
            }
        } else {
            // Write file data, but check for end boundary
            char *boundary_end = strstr(buffer, boundary);
            int write_len = boundary_end ? (boundary_end - buffer - 4) : received; // -4 for \r\n--
            
            if (write_len > 0) {
                if (ota_write_data(buffer, write_len) != ESP_OK) {
                    ota_abort_update();
                    WEBSERVER_SEND_ERROR(req, HTTP_500_INTERNAL_ERROR, "OTA write failed");
                }
            }
            
            if (boundary_end) break; // End of file data
        }
        
        remaining -= received;
    }
    
    // End OTA update
    if (ota_end_update() != ESP_OK) {
        WEBSERVER_SEND_ERROR(req, HTTP_500_INTERNAL_ERROR, "Failed to complete OTA update");
    }
    
    ESP_LOGI(TAG, "OTA upload completed successfully");
    
    httpd_resp_set_type(req, CONTENT_TYPE_PLAIN);
    httpd_resp_send(req, "OTA update completed. Device will restart...", 44);
    
    // Restart after a delay
    vTaskDelay(pdMS_TO_TICKS(3000));
    ota_restart_device();
    
    return ESP_OK;
}

esp_err_t webserver_ota_status_handler(httpd_req_t *req) {
    ota_status_t status = ota_get_status();
    char response[256];
    
    const char *state_str;
    switch (status.state) {
        case OTA_STATE_IDLE: state_str = "idle"; break;
        case OTA_STATE_DOWNLOADING: state_str = "downloading"; break;
        case OTA_STATE_COMPLETE: state_str = "complete"; break;
        case OTA_STATE_ERROR: state_str = "error"; break;
        default: state_str = "unknown"; break;
    }
    
    snprintf(response, sizeof(response),
        "{"
        "\"state\":\"%s\","
        "\"progress\":%d,"
        "\"status\":\"%s\""
        "}",
        state_str,
        status.progress,
        strlen(status.error_message) > 0 ? status.error_message : "OK"
    );
    
    WEBSERVER_SEND_JSON_RESPONSE(req, response);
}

void webserver_ota_register_handlers(httpd_handle_t server) {
    // Register OTA upload handler
    httpd_uri_t ota_upload_uri = {
        .uri = "/ota-upload",
        .method = HTTP_POST,
        .handler = webserver_ota_upload_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_upload_uri);
    
    // Register OTA status handler  
    httpd_uri_t ota_status_uri = {
        .uri = "/ota-status",
        .method = HTTP_GET,
        .handler = webserver_ota_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_status_uri);
    
    ESP_LOGI(TAG, "OTA handlers registered");
}
