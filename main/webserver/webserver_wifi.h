#pragma once

#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi network scan handler
 * 
 * Scans for available WiFi networks and returns JSON response
 */
esp_err_t webserver_wifi_scan_handler(httpd_req_t *req);

/**
 * @brief WiFi connection handler
 * 
 * Handles WiFi connection requests with SSID and password
 */
esp_err_t webserver_wifi_connect_handler(httpd_req_t *req);

/**
 * @brief WiFi disconnect handler
 * 
 * Disconnects from current WiFi network and switches to hotspot mode
 */
esp_err_t webserver_wifi_disconnect_handler(httpd_req_t *req);

/**
 * @brief WiFi forget handler
 * 
 * Forgets saved WiFi credentials and switches to hotspot mode
 */
esp_err_t webserver_wifi_forget_handler(httpd_req_t *req);

/**
 * @brief Register WiFi handlers with the web server
 * 
 * Registers all WiFi-related HTTP handlers
 */
void webserver_wifi_register_handlers(httpd_handle_t server);

#ifdef __cplusplus
}
#endif
