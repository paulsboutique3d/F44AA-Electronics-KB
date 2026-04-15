/**
 * @file wifi_config.c
 * @brief WiFi Configuration and Connection Manager
 * 
 * Manages WiFi connectivity including Access Point mode for initial
 * setup and Station mode for connecting to existing networks.
 * 
 * Features:
 * - Access Point mode (SSID: F44AA, Pass: aliens1986)
 * - Station mode with credential storage in NVS
 * - Network scanning and connection management
 * - Automatic reconnection on signal loss
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 * 
 * Default AP: SSID=F44AA, Password=aliens1986, IP=192.168.4.1
 */

#include "wifi_config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "WiFi_Config";

// WiFi connection status
static bool wifi_sta_connected = false;
static bool wifi_ap_active = true;
static char connected_ssid[32] = "";
static char current_ip[16] = "192.168.4.1";  // Default AP IP

// Event group for WiFi events
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT = BIT1;

// NVS keys for WiFi credentials
#define NVS_NAMESPACE "wifi_config"
#define NVS_SSID_KEY "sta_ssid"
#define NVS_PASS_KEY "sta_pass"
#define NVS_MODE_KEY "wifi_mode"

static int sta_retry_num = 0;
#define MAX_STA_RETRY 5

// WiFi event handler
static void wifi_config_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi station started, attempting connection...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*) event_data;
        ESP_LOGI(TAG, "WiFi station connected to SSID: %s, Channel: %d", 
                 event->ssid, event->channel);
        ESP_LOGI(TAG, "Waiting for IP address assignment...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "WiFi station disconnected from SSID: %s, Reason: %d", 
                 event->ssid, event->reason);
        
        if (sta_retry_num < MAX_STA_RETRY) {
            esp_wifi_connect();
            sta_retry_num++;
            ESP_LOGI(TAG, "Retry connecting to WiFi network (%d/%d)", sta_retry_num, MAX_STA_RETRY);
        } else {
            ESP_LOGI(TAG, "Failed to connect to WiFi after %d attempts, switching back to AP mode", MAX_STA_RETRY);
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            wifi_config_switch_to_ap_mode();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "🌐 SUCCESS! Connected to WiFi with IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Gateway: " IPSTR ", Netmask: " IPSTR, 
                 IP2STR(&event->ip_info.gw), IP2STR(&event->ip_info.netmask));
        
        snprintf(current_ip, sizeof(current_ip), IPSTR, IP2STR(&event->ip_info.ip));
        sta_retry_num = 0;
        wifi_sta_connected = true;
        // Keep AP active even when STA is connected - dual mode operation
        wifi_ap_active = true; // Changed: Keep AP active for dual operation
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        
        ESP_LOGI(TAG, "F44AA Web Interface now available at: http://%s", current_ip);
        ESP_LOGI(TAG, "📶 AP hotspot still active: F44AA at 192.168.4.1");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
        ESP_LOGW(TAG, "WiFi station lost IP address");
        wifi_sta_connected = false;
        strcpy(current_ip, "192.168.4.1"); // Fall back to AP IP
    }
}

esp_err_t wifi_config_init(void) {
    ESP_LOGI(TAG, "Initializing WiFi configuration");
    
    // Create event group for WiFi events
    wifi_event_group = xEventGroupCreate();
    if (wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create WiFi event group");
        return ESP_ERR_NO_MEM;
    }
    
    // Note: esp_netif_init() and esp_event_loop_create_default() are already called in main.c
    // Don't initialize netif here - it's already done in main.c
    
    // Don't create default WiFi interfaces here - they should be created only when needed
    // The webserver will handle AP creation when starting the access point
    
    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register WiFi event handlers
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                             ESP_EVENT_ANY_ID,
                                             &wifi_config_event_handler,
                                             NULL,
                                             NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_event_handler_instance_register(IP_EVENT,
                                             ESP_EVENT_ANY_ID,
                                             &wifi_config_event_handler,
                                             NULL,
                                             NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "WiFi configuration initialized successfully");
    return ESP_OK;
}

esp_err_t wifi_config_save_credentials(const char* ssid, const char* password) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_str(nvs_handle, NVS_SSID_KEY, ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    err = nvs_set_str(nvs_handle, NVS_PASS_KEY, password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi credentials saved: SSID=%s", ssid);
    }
    
    return err;
}

esp_err_t wifi_config_load_credentials(char* ssid, size_t ssid_len, char* password, size_t pass_len) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No stored WiFi credentials found");
        return err;
    }
    
    err = nvs_get_str(nvs_handle, NVS_SSID_KEY, ssid, &ssid_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error loading SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    err = nvs_get_str(nvs_handle, NVS_PASS_KEY, password, &pass_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error loading password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "WiFi credentials loaded: SSID=%s", ssid);
    return ESP_OK;
}

esp_err_t wifi_config_connect_to_network(const char* ssid, const char* password) {
    ESP_LOGI(TAG, "Attempting to connect to WiFi network: %s (keeping AP active)", ssid);
    
    // Save credentials first
    wifi_config_save_credentials(ssid, password);
    strcpy(connected_ssid, ssid);
    
    // Reset retry counter
    sta_retry_num = 0;
    
    // Clear previous event group bits
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    
    // Ensure WiFi is in APSTA mode (keep AP active while connecting)
    wifi_mode_t current_mode;
    esp_wifi_get_mode(&current_mode);
    if (current_mode != WIFI_MODE_APSTA) {
        ESP_LOGI(TAG, "Setting WiFi to APSTA mode (AP + STA)");
        esp_err_t mode_ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (mode_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set APSTA mode: %s", esp_err_to_name(mode_ret));
            return mode_ret;
        }
        vTaskDelay(pdMS_TO_TICKS(500)); // Wait for mode switch
    }
    
    // Disconnect from any existing STA connection first
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(500)); // Give time for disconnection
    
    wifi_config_t sta_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    strcpy((char*)sta_config.sta.ssid, ssid);
    strcpy((char*)sta_config.sta.password, password);
    
    ESP_LOGI(TAG, "Setting STA configuration for SSID: %s", ssid);
    esp_err_t config_ret = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    if (config_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set STA config: %s", esp_err_to_name(config_ret));
        return config_ret;
    }
    
    // Connect to the network (WiFi should already be started in APSTA mode)
    ESP_LOGI(TAG, "Connecting to WiFi network (AP remains active)...");
    esp_err_t connect_err = esp_wifi_connect();
    if (connect_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initiate WiFi connection: %s", esp_err_to_name(connect_err));
        return connect_err;
    }
    
    ESP_LOGI(TAG, "WiFi connection initiated, waiting for result...");
    
    // Wait for connection or failure (with timeout)
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          pdMS_TO_TICKS(20000)); // 20 second timeout
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "✅ Successfully connected to WiFi network: %s", ssid);
        ESP_LOGI(TAG, "🌐 Device IP address: %s", current_ip);
        ESP_LOGI(TAG, "📶 AP hotspot remains active: F44AA");
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "❌ Failed to connect to WiFi network: %s", ssid);
        ESP_LOGI(TAG, "📶 AP hotspot remains active for fallback");
        return ESP_FAIL;
    } else {
        ESP_LOGW(TAG, "⏱️ WiFi connection timeout for network: %s", ssid);
        ESP_LOGI(TAG, "📶 AP hotspot remains active for fallback");
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t wifi_config_switch_to_ap_mode(void) {
    ESP_LOGI(TAG, "Ensuring AP mode is active");
    
    wifi_sta_connected = false;
    wifi_ap_active = true;
    strcpy(connected_ssid, "");
    
    // Get current mode and only change if necessary
    wifi_mode_t current_mode;
    esp_wifi_get_mode(&current_mode);
    
    if (current_mode == WIFI_MODE_APSTA) {
        ESP_LOGI(TAG, "Already in APSTA mode, disconnecting STA only");
        esp_wifi_disconnect(); // Just disconnect STA, keep APSTA mode
        strcpy(current_ip, "192.168.4.1"); // Reset to AP IP
    } else if (current_mode != WIFI_MODE_AP) {
        ESP_LOGI(TAG, "Switching to AP mode");
        // Stop current WiFi and restart in AP mode
        esp_wifi_stop();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_start());
        strcpy(current_ip, "192.168.4.1");
    }
    
    ESP_LOGI(TAG, "AP mode ensured - F44AA active at 192.168.4.1");
    return ESP_OK;
}

esp_err_t wifi_config_disconnect_and_clear(void) {
    ESP_LOGI(TAG, "Disconnecting from WiFi and clearing credentials");
    
    // Clear stored credentials
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_erase_key(nvs_handle, NVS_SSID_KEY);
        nvs_erase_key(nvs_handle, NVS_PASS_KEY);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
    
    // Switch back to AP mode
    wifi_config_switch_to_ap_mode();
    
    return ESP_OK;
}

bool wifi_config_is_connected_to_network(void) {
    return wifi_sta_connected;
}

bool wifi_config_is_ap_active(void) {
    return wifi_ap_active;
}

const char* wifi_config_get_current_ip(void) {
    return current_ip;
}

const char* wifi_config_get_connected_ssid(void) {
    return connected_ssid;
}

esp_err_t wifi_config_scan_networks(wifi_scan_result_t* results, uint16_t* count) {
    ESP_LOGI(TAG, "Scanning for WiFi networks...");
    
    // Ensure WiFi is in APSTA mode for scanning
    wifi_mode_t current_mode;
    esp_wifi_get_mode(&current_mode);
    
    if (current_mode != WIFI_MODE_APSTA && current_mode != WIFI_MODE_STA) {
        ESP_LOGI(TAG, "Switching to APSTA mode for scanning");
        esp_err_t mode_err = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (mode_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set APSTA mode: %s", esp_err_to_name(mode_err));
            return mode_err;
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for mode switch
    }
    
    // If currently connecting/connected, disconnect temporarily for scan
    bool was_connecting = false;
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        ESP_LOGI(TAG, "Temporarily disconnecting from %s for scan", (char*)ap_info.ssid);
        esp_wifi_disconnect();
        was_connecting = true;
        vTaskDelay(pdMS_TO_TICKS(500)); // Wait for disconnection
    }
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 100,
                .max = 300
            }
        }
    };
    
    ESP_LOGI(TAG, "Starting WiFi scan...");
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan start failed: %s", esp_err_to_name(err));
        return err;
    }
    
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    ESP_LOGI(TAG, "Scan completed, found %d networks", ap_count);
    
    if (ap_count == 0) {
        ESP_LOGW(TAG, "No WiFi networks found");
        *count = 0;
        return ESP_OK;
    }
    
    // Limit to maximum requested count
    if (ap_count > *count) {
        ap_count = *count;
    }
    
    wifi_ap_record_t* ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_records == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for scan results");
        return ESP_ERR_NO_MEM;
    }
    
    err = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get scan records: %s", esp_err_to_name(err));
        free(ap_records);
        return err;
    }
    
    // Convert to our result structure
    for (int i = 0; i < ap_count; i++) {
        strncpy(results[i].ssid, (char*)ap_records[i].ssid, sizeof(results[i].ssid) - 1);
        results[i].ssid[sizeof(results[i].ssid) - 1] = '\0'; // Ensure null termination
        results[i].rssi = ap_records[i].rssi;
        results[i].authmode = ap_records[i].authmode;
        ESP_LOGI(TAG, "Network %d: %s (RSSI: %d, Auth: %d)", 
                 i, results[i].ssid, results[i].rssi, results[i].authmode);
    }
    
    *count = ap_count;
    free(ap_records);
    
    ESP_LOGI(TAG, "Successfully scanned %d WiFi networks", ap_count);
    return ESP_OK;
}
esp_err_t wifi_config_try_auto_connect(void) {
    char ssid[32];
    char password[64];
    
    esp_err_t err = wifi_config_load_credentials(ssid, sizeof(ssid), password, sizeof(password));
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No stored WiFi credentials, staying in AP mode");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Found stored credentials, attempting auto-connect to: %s", ssid);
    return wifi_config_connect_to_network(ssid, password);
}

esp_err_t wifi_config_init_ap(void) {
    ESP_LOGI(TAG, "Initializing WiFi Access Point in APSTA mode");
    
    // Create both AP and STA network interfaces
    esp_netif_t* netif_ap = esp_netif_create_default_wifi_ap();
    esp_netif_t* netif_sta = esp_netif_create_default_wifi_sta();
    
    if (netif_ap == NULL || netif_sta == NULL) {
        ESP_LOGE(TAG, "Failed to create WiFi network interfaces");
        return ESP_FAIL;
    }
    
    // Set WiFi mode to APSTA (Access Point + Station) to allow scanning
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode to APSTA: %s", esp_err_to_name(ret));
        return ret;
    }
    
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = F44AA_AP_SSID,
            .password = F44AA_AP_PASS,
            .ssid_len = strlen(F44AA_AP_SSID),
            .channel = F44AA_AP_CHANNEL,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = F44AA_AP_MAX_CONN,
        },
    };
    
    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set AP config: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "WiFi Access Point started in APSTA mode");
    ESP_LOGI(TAG, "SSID: %s", F44AA_AP_SSID);
    ESP_LOGI(TAG, "Password: %s", F44AA_AP_PASS);
    ESP_LOGI(TAG, "Channel: %d", F44AA_AP_CHANNEL);
    ESP_LOGI(TAG, "Web interface: http://192.168.4.1");
    
    return ESP_OK;
}
