#include "webserver_wifi.h"
#include "webserver_common.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "wifi_config.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "webserver_wifi";

esp_err_t webserver_wifi_scan_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "WiFi scan requested");
    
    // Set CORS headers
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");
    
    // Scan for networks
    wifi_scan_result_t scan_results[20];  // Scan up to 20 networks
    uint16_t count = 20;
    
    esp_err_t scan_err = wifi_config_scan_networks(scan_results, &count);
    if (scan_err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(scan_err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
        return ESP_FAIL;
    }
    
    // Build JSON response
    char* response = malloc(2048);  // Reduced from 4096 to 2048 bytes
    if (response == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for scan response");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }
    
    strcpy(response, "{\"networks\":[");
    
    for (int i = 0; i < count; i++) {
        char network_entry[200];
        const char* auth_str = "Open";
        
        // Convert auth mode to string
        switch (scan_results[i].authmode) {
            case WIFI_AUTH_OPEN: auth_str = "Open"; break;
            case WIFI_AUTH_WEP: auth_str = "WEP"; break;
            case WIFI_AUTH_WPA_PSK: auth_str = "WPA"; break;
            case WIFI_AUTH_WPA2_PSK: auth_str = "WPA2"; break;
            case WIFI_AUTH_WPA_WPA2_PSK: auth_str = "WPA/WPA2"; break;
            case WIFI_AUTH_WPA3_PSK: auth_str = "WPA3"; break;
            default: auth_str = "Unknown"; break;
        }
        
        snprintf(network_entry, sizeof(network_entry),
                "%s{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":\"%s\",\"secure\":%s}",
                (i > 0) ? "," : "",
                scan_results[i].ssid,
                scan_results[i].rssi,
                auth_str,
                (scan_results[i].authmode == WIFI_AUTH_OPEN) ? "false" : "true");
        
        strcat(response, network_entry);
    }
    
    strcat(response, "]}");
    
    ESP_LOGI(TAG, "Sending scan response with %d networks", count);
    httpd_resp_send(req, response, strlen(response));
    
    free(response);
    return ESP_OK;
}

esp_err_t webserver_wifi_connect_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "WiFi connect handler called - Content-Length: %zu", req->content_len);
    
    char content[1024];  // Buffer for form data
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        ESP_LOGE(TAG, "Failed to receive wifi connect data, ret=%d", ret);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Invalid request", 15);
    }
    content[ret] = '\0';
    
    ESP_LOGI(TAG, "Received WiFi connect data (%d bytes): %s", ret, content);
    
    char ssid[33] = "";
    char password[65] = "";
    
    // Parse form data using URL-encoded format: ssid=NetworkName&password=MyPassword
    char *token = strtok(content, "&");
    while (token != NULL) {
        ESP_LOGI(TAG, "Parsing token: %s", token);
        
        if (strncmp(token, "ssid=", 5) == 0) {
            // URL decode SSID
            char *encoded_ssid = token + 5;
            int j = 0;
            for (int i = 0; encoded_ssid[i] && j < sizeof(ssid) - 1; i++) {
                if (encoded_ssid[i] == '%' && encoded_ssid[i+1] && encoded_ssid[i+2]) {
                    // Simple URL decode for common characters
                    if (strncmp(encoded_ssid + i, "%20", 3) == 0) {
                        ssid[j++] = ' ';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%21", 3) == 0) {
                        ssid[j++] = '!';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%40", 3) == 0) {
                        ssid[j++] = '@';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%23", 3) == 0) {
                        ssid[j++] = '#';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%24", 3) == 0) {
                        ssid[j++] = '$';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%25", 3) == 0) {
                        ssid[j++] = '%';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%5E", 3) == 0) {
                        ssid[j++] = '^';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%26", 3) == 0) {
                        ssid[j++] = '&';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%2A", 3) == 0) {
                        ssid[j++] = '*';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%28", 3) == 0) {
                        ssid[j++] = '(';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%29", 3) == 0) {
                        ssid[j++] = ')';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%2D", 3) == 0) {
                        ssid[j++] = '-';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%5F", 3) == 0) {
                        ssid[j++] = '_';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%3D", 3) == 0) {
                        ssid[j++] = '=';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%2B", 3) == 0) {
                        ssid[j++] = '+';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%5B", 3) == 0) {
                        ssid[j++] = '[';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%5D", 3) == 0) {
                        ssid[j++] = ']';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%7B", 3) == 0) {
                        ssid[j++] = '{';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%7D", 3) == 0) {
                        ssid[j++] = '}';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%7C", 3) == 0) {
                        ssid[j++] = '|';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%5C", 3) == 0) {
                        ssid[j++] = '\\';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%3A", 3) == 0) {
                        ssid[j++] = ':';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%3B", 3) == 0) {
                        ssid[j++] = ';';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%22", 3) == 0) {
                        ssid[j++] = '"';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%27", 3) == 0) {
                        ssid[j++] = '\'';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%3C", 3) == 0) {
                        ssid[j++] = '<';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%3E", 3) == 0) {
                        ssid[j++] = '>';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%2C", 3) == 0) {
                        ssid[j++] = ',';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%2E", 3) == 0) {
                        ssid[j++] = '.';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%2F", 3) == 0) {
                        ssid[j++] = '/';
                        i += 2;
                    } else if (strncmp(encoded_ssid + i, "%3F", 3) == 0) {
                        ssid[j++] = '?';
                        i += 2;
                    } else {
                        ssid[j++] = encoded_ssid[i];
                    }
                } else if (encoded_ssid[i] == '+') {
                    ssid[j++] = ' ';  // URL encoding uses + for space
                } else {
                    ssid[j++] = encoded_ssid[i];
                }
            }
            ssid[j] = '\0';
            ESP_LOGI(TAG, "Parsed SSID: '%s'", ssid);
        }
        else if (strncmp(token, "password=", 9) == 0) {
            strncpy(password, token + 9, sizeof(password) - 1);
            password[sizeof(password) - 1] = '\0';
            ESP_LOGI(TAG, "Parsed password: [%zu chars]", strlen(password));
        }
        
        token = strtok(NULL, "&");
    }
    
    if (strlen(ssid) == 0) {
        ESP_LOGE(TAG, "No SSID provided");
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "SSID required", 13);
    }
    
    ESP_LOGI(TAG, "Attempting to connect to SSID: '%s'", ssid);
    
    // Try to connect to the network
    esp_err_t result = wifi_config_connect_to_network(ssid, password);
    
    const char* response_msg;
    int response_len;
    
    switch (result) {
        case ESP_OK:
            response_msg = "Connected successfully";
            response_len = 22;
            ESP_LOGI(TAG, "WiFi connection successful");
            break;
        case ESP_ERR_INVALID_ARG:
            httpd_resp_set_status(req, "400 Bad Request");
            response_msg = "Invalid parameters";
            response_len = 18;
            ESP_LOGW(TAG, "WiFi connection failed: Invalid parameters");
            break;
        case ESP_FAIL:
        default:
            httpd_resp_set_status(req, "500 Internal Server Error");
            response_msg = "Connection failed";
            response_len = 17;
            ESP_LOGE(TAG, "WiFi connection failed: %s", esp_err_to_name(result));
            break;
    }
    
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, response_msg, response_len);
}

esp_err_t webserver_wifi_disconnect_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Disconnecting from WiFi network");
    wifi_config_disconnect_and_clear();
    
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "Disconnected from WiFi. Using hotspot mode.", 43);
}

esp_err_t webserver_wifi_forget_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Forgetting saved WiFi credentials");
    wifi_config_disconnect_and_clear();
    
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "Saved WiFi credentials cleared. Using hotspot mode.", 51);
}

void webserver_wifi_register_handlers(httpd_handle_t server) {
    // Register WiFi scan handler
    httpd_uri_t wifi_scan_uri = {
        .uri = "/wifi-scan",
        .method = HTTP_GET,
        .handler = webserver_wifi_scan_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &wifi_scan_uri);
    
    // Register WiFi connect handler
    httpd_uri_t wifi_connect_uri = {
        .uri = "/wifi-connect",
        .method = HTTP_POST,
        .handler = webserver_wifi_connect_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &wifi_connect_uri);
    
    // Register WiFi disconnect handler
    httpd_uri_t wifi_disconnect_uri = {
        .uri = "/wifi-disconnect",
        .method = HTTP_GET,
        .handler = webserver_wifi_disconnect_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &wifi_disconnect_uri);
    
    // Register WiFi forget handler
    httpd_uri_t wifi_forget_uri = {
        .uri = "/wifi-forget",
        .method = HTTP_GET,
        .handler = webserver_wifi_forget_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &wifi_forget_uri);
    
    ESP_LOGI(TAG, "WiFi handlers registered");
}
