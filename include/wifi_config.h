/**
 * @file wifi_config.h
 * @brief WiFi Configuration Interface
 * 
 * WiFi AP/Station mode management.
 * Default AP: SSID=F44AA, Password=aliens1986
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_wifi.h"
#include <stdint.h>

// F44AA WiFi Access Point Configuration
#define F44AA_AP_SSID      "F44AA"
#define F44AA_AP_PASS      "aliens1986"
#define F44AA_AP_CHANNEL   1
#define F44AA_AP_MAX_CONN  4

// WiFi scan result structure
typedef struct {
    char ssid[33];
    int8_t rssi;
    wifi_auth_mode_t authmode;
} wifi_scan_result_t;

// WiFi configuration functions
esp_err_t wifi_config_init(void);
esp_err_t wifi_config_init_ap(void);
esp_err_t wifi_config_save_credentials(const char* ssid, const char* password);
esp_err_t wifi_config_load_credentials(char* ssid, size_t ssid_len, char* password, size_t pass_len);
esp_err_t wifi_config_connect_to_network(const char* ssid, const char* password);
esp_err_t wifi_config_switch_to_ap_mode(void);
esp_err_t wifi_config_disconnect_and_clear(void);
esp_err_t wifi_config_try_auto_connect(void);

// WiFi status functions
bool wifi_config_is_connected_to_network(void);
bool wifi_config_is_ap_active(void);
const char* wifi_config_get_current_ip(void);
const char* wifi_config_get_connected_ssid(void);

// WiFi scanning
esp_err_t wifi_config_scan_networks(wifi_scan_result_t* results, uint16_t* count);
