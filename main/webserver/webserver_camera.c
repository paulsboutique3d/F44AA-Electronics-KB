#include "webserver_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "img_converters.h"

static const char *TAG = "WebServer_Camera";

// Test JPEG data for when camera is not available
static const uint8_t test_jpeg[] = {
    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43,
    0x00, 0x08, 0x06, 0x06, 0x07, 0x06, 0x05, 0x08, 0x07, 0x07, 0x07, 0x09,
    0x09, 0x08, 0x0A, 0x0C, 0x14, 0x0D, 0x0C, 0x0B, 0x0B, 0x0C, 0x19, 0x12,
    0x13, 0x0F, 0x14, 0x1D, 0x1A, 0x1F, 0x1E, 0x1D, 0x1A, 0x1C, 0x1C, 0x20,
    0x24, 0x2E, 0x27, 0x20, 0x22, 0x2C, 0x23, 0x1C, 0x1C, 0x28, 0x37, 0x29,
    0x2C, 0x30, 0x31, 0x34, 0x34, 0x34, 0x1F, 0x27, 0x39, 0x3D, 0x38, 0x32,
    0x3C, 0x2E, 0x33, 0x34, 0x32, 0xFF, 0xC0, 0x00, 0x11, 0x08, 0x00, 0x64,
    0x00, 0x64, 0x03, 0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01,
    0xFF, 0xC4, 0x00, 0x1F, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
    0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0xFF, 0xDA, 0x00,
    0x08, 0x01, 0x01, 0x00, 0x00, 0x3F, 0x00, 0xD2, 0xCF, 0x20, 0xFF, 0xD9
};

esp_err_t webserver_camera_send_test_image(httpd_req_t *req) {
    // Set headers for JPEG image
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"F44AA_test_image.jpg\"");
    webserver_set_cors_headers(req);
    
    // Send test image data
    esp_err_t res = httpd_resp_send(req, (const char *)test_jpeg, sizeof(test_jpeg));
    
    ESP_LOGI(TAG, "Test image sent, size: %zu bytes", sizeof(test_jpeg));
    return res;
}

esp_err_t webserver_camera_stream_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Camera stream requested");
    
    if (!camera_is_initialized()) {
        ESP_LOGW(TAG, "Camera not initialized - providing test pattern stream");
        
        // Set headers for MJPEG streaming
        httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
        webserver_set_cors_headers(req);
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_set_hdr(req, "Pragma", "no-cache");
        
        // Send test pattern as a stream with improved error handling
        for (int i = 0; i < 200; i++) {  // Stream more frames for continuous test
            // Check connection health early and often
            if (i > 0 && i % 5 == 0) {
                char test_chunk[] = "";
                if (httpd_resp_send_chunk(req, test_chunk, 0) != ESP_OK) {
                    ESP_LOGI(TAG, "Client disconnected, stopping test stream after %d frames", i);
                    return ESP_OK;  // Clean exit, not an error
                }
            }
            
            // Send MJPEG frame boundary with proper formatting
            const char *boundary = (i == 0) ? "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " : 
                                             "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ";
            esp_err_t res = httpd_resp_send_chunk(req, boundary, strlen(boundary));
            if (res != ESP_OK) {
                ESP_LOGI(TAG, "Client disconnected at boundary (test frame %d)", i);
                return ESP_OK;  // Clean exit
            }
            
            // Send content length
            char content_length[32];
            snprintf(content_length, sizeof(content_length), "%zu\r\n\r\n", sizeof(test_jpeg));
            res = httpd_resp_send_chunk(req, content_length, strlen(content_length));
            if (res != ESP_OK) {
                ESP_LOGI(TAG, "Client disconnected at content-length (test frame %d)", i);
                return ESP_OK;  // Clean exit
            }
            
            // Send the test image data
            res = httpd_resp_send_chunk(req, (const char *)test_jpeg, sizeof(test_jpeg));
            if (res != ESP_OK) {
                ESP_LOGI(TAG, "Client disconnected at image data (test frame %d)", i);
                return ESP_OK;  // Clean exit
            }
            
            ESP_LOGD(TAG, "Sent test frame %d, size: %zu bytes", i, sizeof(test_jpeg));
            
            // Slower frame rate for better browser compatibility (2 FPS)
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        
        ESP_LOGI(TAG, "Test pattern stream completed successfully");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting MJPEG camera stream");
    
    // Set headers for MJPEG streaming
    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
    webserver_set_cors_headers(req);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    
    esp_err_t res = ESP_OK;
    size_t frame_count = 0;
    
    while (true) {
        camera_fb_t *fb = NULL;
        camera_err_t camera_ret = camera_capture(&fb);
        
        if (camera_ret != CAMERA_OK || fb == NULL) {
            ESP_LOGW(TAG, "Camera capture failed in stream (frame %zu)", frame_count);
            break;
        }
        
        // For ESP32-S3-CAM, we expect JPEG format directly from camera
        const uint8_t *image_buf = fb->buf;
        size_t image_len = fb->len;
        
        // Only convert if it's not already JPEG
        uint8_t *jpeg_buf = NULL;
        size_t jpeg_len = 0;
        bool converted = false;
        
        if (fb->format != PIXFORMAT_JPEG) {
            ESP_LOGW(TAG, "Converting frame from format %d to JPEG", fb->format);
            converted = fmt2jpg(fb->buf, fb->len, fb->width, fb->height, fb->format, 80, &jpeg_buf, &jpeg_len);
            if (!converted) {
                ESP_LOGW(TAG, "Failed to convert frame to JPEG (frame %zu)", frame_count);
                camera_return_fb(fb);
                continue;
            }
            image_buf = jpeg_buf;
            image_len = jpeg_len;
        }
        
        // Send MJPEG frame boundary with proper formatting
        const char *boundary = (frame_count == 0) ? "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " : 
                                                   "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ";
        res = httpd_resp_send_chunk(req, boundary, strlen(boundary));
        if (res != ESP_OK) {
            ESP_LOGI(TAG, "Client disconnected at boundary (frame %zu)", frame_count);
            if (converted && jpeg_buf) free(jpeg_buf);
            camera_return_fb(fb);
            return ESP_OK;  // Clean exit
        }
        
        // Send content length
        char content_length[32];
        snprintf(content_length, sizeof(content_length), "%zu\r\n\r\n", image_len);
        res = httpd_resp_send_chunk(req, content_length, strlen(content_length));
        if (res != ESP_OK) {
            ESP_LOGI(TAG, "Client disconnected at content-length (frame %zu)", frame_count);
            if (converted && jpeg_buf) free(jpeg_buf);
            camera_return_fb(fb);
            return ESP_OK;  // Clean exit
        }
        
        // Send the image data
        res = httpd_resp_send_chunk(req, (const char *)image_buf, image_len);
        if (res != ESP_OK) {
            ESP_LOGI(TAG, "Client disconnected at image data (frame %zu)", frame_count);
            if (converted && jpeg_buf) free(jpeg_buf);
            camera_return_fb(fb);
            return ESP_OK;  // Clean exit
        }
        
        ESP_LOGD(TAG, "Sent frame %zu, size: %zu bytes", frame_count, image_len);
        frame_count++;
        
        // Clean up
        camera_return_fb(fb);
        if (converted && jpeg_buf) {
            free(jpeg_buf);
        }
        
        // Small delay between frames for ~10 FPS
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Check if we should stop (every 10 frames to reduce overhead)
        if (frame_count % 10 == 0) {
            // Simple way to check if connection is still alive
            // If we can't send an empty chunk, connection is probably closed
            char test_chunk[] = "";
            if (httpd_resp_send_chunk(req, test_chunk, 0) != ESP_OK) {
                ESP_LOGI(TAG, "Client disconnected, stopping stream after %zu frames", frame_count);
                return ESP_OK;  // Clean exit, not an error
            }
        }
    }
    
    ESP_LOGI(TAG, "Camera stream ended after %zu frames", frame_count);
    return res;
}

esp_err_t webserver_camera_page_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Camera page requested - URI: %s", req->uri);
    
    const char* camera_html = 
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <title>F44AA Camera Stream</title>\n"
        "    <meta charset='UTF-8'>\n"
        "    <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n"
        "    <style>\n"
        "        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }\n"
        "        .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }\n"
        "        h1 { color: #333; text-align: center; }\n"
        "        .stream-container { text-align: center; margin: 20px 0; }\n"
        "        .camera-stream { max-width: 100%; height: auto; border: 2px solid #ddd; border-radius: 5px; }\n"
        "        .status { text-align: center; color: #666; margin: 10px 0; }\n"
        "        .status.warning { color: #ff6600; font-weight: bold; }\n"
        "        .controls { text-align: center; margin: 20px 0; }\n"
        "        button { padding: 10px 20px; margin: 5px; background: #007bff; color: white; border: none; border-radius: 5px; cursor: pointer; }\n"
        "        button:hover { background: #0056b3; }\n"
        "        .info { background: #e9ecef; padding: 15px; border-radius: 5px; margin: 20px 0; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <div class='container'>\n"
        "        <h1>F44AA Camera Stream</h1>\n"
        "        <div class='info'>\n"
        "            <strong>Camera Status:</strong> Hardware initialization failed.<br>\n"
        "            <strong>Fallback Mode:</strong> Showing test pattern stream.<br>\n"
        "            <strong>Issue:</strong> OV2640 sensor not detected on I2C bus.\n"
        "        </div>\n"
        "        <div class='status warning' id='status'>Loading test pattern stream...</div>\n"
        "        <div class='stream-container'>\n"
        "            <img id='camera-stream' class='camera-stream' src='/camera-stream' \n"
        "                 onerror=\"document.getElementById('status').innerText='Stream connection failed';\"\n"
        "                 onload=\"document.getElementById('status').innerText='Test pattern stream active (camera hardware offline)';\">\n"
        "        </div>\n"
        "        <div class='controls'>\n"
        "            <button onclick=\"document.getElementById('camera-stream').src='/camera-stream?t='+Date.now()\">\n"
        "                Reload Stream\n"
        "            </button>\n"
        "            <button onclick=\"window.open('/camera-capture', '_blank')\">\n"
        "                Test Image Download\n"
        "            </button>\n"
        "            <button onclick=\"window.location.href='/'\">\n"
        "                Back to Main Interface\n"
        "            </button>\n"
        "        </div>\n"
        "        <div class='info'>\n"
        "            <strong>Troubleshooting:</strong><br>\n"
        "            - Check camera module connection<br>\n"
        "            - Verify power supply to camera<br>\n"
        "            - Ensure correct ESP32-S3-CAM variant with OV2640\n"
        "        </div>\n"
        "    </div>\n"
        "</body>\n"
        "</html>";
    
    httpd_resp_set_type(req, CONTENT_TYPE_HTML);
    webserver_set_cors_headers(req);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    
    return httpd_resp_send(req, camera_html, strlen(camera_html));
}

esp_err_t webserver_camera_capture_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Camera capture requested");
    
    if (!camera_is_initialized()) {
        ESP_LOGW(TAG, "Camera not initialized, sending test image");
        return webserver_camera_send_test_image(req);
    }
    
    camera_fb_t *fb = NULL;
    camera_err_t camera_ret = camera_capture(&fb);
    
    if (camera_ret == CAMERA_CAPTURE_FAIL || fb == NULL) {
        ESP_LOGW(TAG, "Camera capture failed - sending test image instead");
        return webserver_camera_send_test_image(req);
    } else if (camera_ret != CAMERA_OK) {
        ESP_LOGE(TAG, "Camera capture failed with error: %d", camera_ret);
        WEBSERVER_SEND_ERROR(req, HTTP_500_INTERNAL_ERROR, "Capture failed");
    }
    
    // Set headers for JPEG image
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"F44AA_capture.jpg\"");
    webserver_set_cors_headers(req);
    
    // Send image data
    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    camera_return_fb(fb);
    
    ESP_LOGI(TAG, "Photo captured and sent, size: %zu bytes", fb->len);
    
    return res;
}

esp_err_t webserver_camera_quality_handler(httpd_req_t *req) {
    if (!camera_is_initialized()) {
        WEBSERVER_SEND_ERROR(req, HTTP_503_SERVICE_UNAVAILABLE, "Camera not initialized");
    }
    
    // TODO: Implement camera quality control
    // This would involve parsing query parameters and setting camera quality
    
    WEBSERVER_SEND_JSON_RESPONSE(req, "{\"status\":\"Camera quality control not yet implemented\"}");
}

void webserver_camera_register_handlers(httpd_handle_t server) {
    // Register camera stream handler
    httpd_uri_t camera_stream_uri = {
        .uri = "/camera-stream",
        .method = HTTP_GET,
        .handler = webserver_camera_stream_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &camera_stream_uri);
    
    // Register camera page handler
    httpd_uri_t camera_page_uri = {
        .uri = "/camera",
        .method = HTTP_GET,
        .handler = webserver_camera_page_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &camera_page_uri);
    
    // Register camera capture handler
    httpd_uri_t camera_capture_uri = {
        .uri = "/camera-capture",
        .method = HTTP_GET,
        .handler = webserver_camera_capture_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &camera_capture_uri);
    
    // Register camera quality handler
    httpd_uri_t camera_quality_uri = {
        .uri = "/camera-quality",
        .method = HTTP_POST,
        .handler = webserver_camera_quality_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &camera_quality_uri);
    
    ESP_LOGI(TAG, "Camera handlers registered");
}
