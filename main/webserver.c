/**
 * @file webserver.c
 * @brief F44AA Web Interface Server
 * 
 * Provides HTTP server for configuration and control of the F44AA
 * Pulse Rifle via web browser. Aliens movie themed interface.
 * 
 * Features:
 * - Live status display (ammo, magazine, audio mode)
 * - Camera streaming and capture
 * - Audio/volume/Bluetooth configuration
 * - Torch color control with RGB sliders
 * - OTA firmware updates
 * - WiFi network management
 * - Test mode for development
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 * 
 * Endpoints: /, /status, /config, /fire, /camera-stream, /ota-upload, etc.
 */

#include "webserver.h"
#include "webserver/webserver_common.h"
#include "webserver/webserver_camera.h"
#include "webserver/webserver_ota.h"
#include "webserver/webserver_wifi.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_flash.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include "img_converters.h"

#include "counter_display.h"
#include "dfplayer.h"
#include "camera.h"
#include "camera_display.h"
#include "magazine.h"
#include "ota_update.h"
#include "wifi_config.h"
#include "ws2812.h"
#include "bluetooth_transmitter.h"

// Define MIN macro if not already defined
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "F44AA_WebServer";

static httpd_handle_t server = NULL;
static bool server_running = false;

// Test mode firing state
static bool test_firing_active = false;
static esp_timer_handle_t test_fire_timer = NULL;

// Forward declarations

// Test fire timer callback - fires at 900 RPM (66.67ms intervals)
static void test_fire_timer_callback(void* arg) {
    if (!test_firing_active) {
        return;
    }
    
    // Get current ammo count
    int current_ammo = counter_display_get_ammo_count();
    
    if (current_ammo > 0) {
        // Decrement ammo and update LCD display
        counter_display_decrement();
        int new_ammo = counter_display_get_ammo_count();
        ESP_LOGI(TAG, "Test fire: LCD counter decremented %d -> %d", current_ammo, new_ammo);
    } else {
        // Out of ammo - stop firing
        test_firing_active = false;
        if (test_fire_timer != NULL) {
            esp_timer_stop(test_fire_timer);
        }
        ESP_LOGI(TAG, "Test fire: Out of ammo - firing stopped");
    }
}



// Configuration structure
typedef struct {
    int fire_rate_rpm;
    int max_ammo;
    int volume;
    audio_output_mode_t audio_mode;
    bool muzzle_flash_enabled;
    bool torch_enabled;
    uint8_t torch_red;
    uint8_t torch_green;
    uint8_t torch_blue;
    bool test_mode_enabled;
    bool camera_display_enabled;
} f44aa_config_t;

static f44aa_config_t current_config = {
    .fire_rate_rpm = 900,
    .max_ammo = 400,
    .volume = 25,
    .audio_mode = AUDIO_OUTPUT_SPEAKER,
    .muzzle_flash_enabled = true,
    .torch_enabled = false,
    .torch_red = 255,
    .torch_green = 255,
    .torch_blue = 255,
    .test_mode_enabled = false,
    .camera_display_enabled = false  // Default to OFF
};

// HTML Pages - store in flash to save RAM (Compressed & Optimized)

static const char* index_html = 
"<!DOCTYPE html><html><head><title>F44AA PULSE RIFLE</title>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<link rel='stylesheet' type='text/css' href='/style.css'>"
"</head><body>"
"<h1>F44AA PULSE RIFLE</h1>"
"<div class='main-grid'>"
"<div class='column'>"
"<h2>STATUS</h2>"
"<div class='sub-section'>"
"<h3>System Status</h3>"
"<p>Ammo: <span id='ammo'>400</span>/400</p>"
"<p>Magazine: <span id='magazine_status'>LOADED</span></p>"
"<p>Audio: <span id='audiomode_display'>SPEAKER</span></p>"
"<p>Volume: <span id='volume_display'>25</span></p>"
"<p>Bluetooth: <span id='bluetooth_status'>Disabled</span></p>"
"</div>"
"<div class='sub-section'>"
"<h3>Light Status</h3>"
"<p>Torch: <span id='torchStatus'>OFF</span></p>"
"<button id='torchToggle' onclick='toggleTorch()'>TOGGLE TORCH</button>"
"<div class='margin-top'>"
"<p>Torch Color:</p>"
"<div class='torch-color-grid'>"
"<button onclick='setTorchColor(255,0,0)' class='torch-color-button torch-color-red'>RED</button>"
"<button onclick='setTorchColor(0,255,0)' class='torch-color-button torch-color-green'>GREEN</button>"
"<button onclick='setTorchColor(0,0,255)' class='torch-color-button torch-color-blue'>BLUE</button>"
"<button onclick='setTorchColor(255,255,0)' class='torch-color-button torch-color-yellow'>YELLOW</button>"
"<button onclick='setTorchColor(255,0,255)' class='torch-color-button torch-color-magenta'>MAGENTA</button>"
"<button onclick='setTorchColor(0,255,255)' class='torch-color-button torch-color-cyan'>CYAN</button>"
"<button onclick='setTorchColor(255,255,255)' class='torch-color-button torch-color-white'>WHITE</button>"
"<button onclick='setTorchColor(255,165,0)' class='torch-color-button torch-color-orange'>ORANGE</button>"
"</div>"
"<div class='margin-top'>"
"<label>Custom RGB:</label><br>"
"<input type='range' id='redSlider' min='0' max='255' value='255' oninput='updateCustomColor()' class='custom-slider'> R: <span id='redValue'>255</span><br>"
"<input type='range' id='greenSlider' min='0' max='255' value='255' oninput='updateCustomColor()' class='custom-slider'> G: <span id='greenValue'>255</span><br>"
"<input type='range' id='blueSlider' min='0' max='255' value='255' oninput='updateCustomColor()' class='custom-slider'> B: <span id='blueValue'>255</span><br>"
"<button onclick='applyCustomColor()' class='margin-top-5'>APPLY CUSTOM</button>"
"</div>"
"</div>"
"</div>"
"</div>"
"<div class='column'>"
"<h2>CONFIG</h2>"
"<div class='sub-section'>"
"<h3>Audio Configuration</h3>"
"<form id='configForm' action='/config' method='POST' enctype='application/x-www-form-urlencoded'>"
"<label>Volume: <input type='number' id='volume' name='volume' value='25' min='0' max='30'></label><br>"
"<label>Audio: <select id='audiomode_select' name='audiomode'><option value='0'>SPEAKER</option><option value='1'>LINE OUT</option></select></label><br>"
"<label>Camera Display: <select id='camdisplay_select' name='camdisplay'><option value='0'>OFF</option><option value='1'>ON</option></select></label><br>"
"<button type='button' onclick='updateConfigFallback()'>UPDATE</button>"
"</form>"
"</div>"
"<div class='sub-section'>"
"<h3>Display Color</h3>"
"<p>Current: <span id='currentColor'>0xF800</span></p>"
"<label>RGB565 Hex: <input type='text' id='colorInput' value='F800' maxlength='4' class='color-input-small'></label>"
"<button onclick='setDisplayColor()'>SET COLOR</button><br><br>"
"<input type='range' id='colorSlider' min='0' max='65535' value='63488' onchange='updateColorFromSlider(this.value)'>"
"<span id='colorDisplay'>0xF800</span><br>"
"<div class='display-color-grid'>"
"<button onclick='setPresetColor(\"F800\")' class='torch-color-button display-color-red'>RED</button>"
"<button onclick='setPresetColor(\"07E0\")' class='torch-color-button display-color-green'>GREEN</button>"
"<button onclick='setPresetColor(\"001F\")' class='torch-color-button display-color-blue'>BLUE</button>"
"<button onclick='setPresetColor(\"FFE0\")' class='torch-color-button display-color-yellow'>YELLOW</button>"
"<button onclick='setPresetColor(\"F81F\")' class='torch-color-button display-color-magenta'>MAGENTA</button>"
"<button onclick='setPresetColor(\"07FF\")' class='torch-color-button display-color-cyan'>CYAN</button>"
"<button onclick='setPresetColor(\"FFFF\")' class='torch-color-button display-color-white'>WHITE</button>"
"</div>"
"</div>"
"<div class='sub-section'>"
"<h3>Bluetooth Logs</h3>"
"<div class='log bluetooth-logs' id='bluetoothLogs'>No Bluetooth activity</div>"
"<button onclick='updateBluetoothLogs()'>REFRESH</button>"
"<button onclick='clearBluetoothLogs()'>CLEAR</button>"
"</div>"
"</div>"
"<div class='column'>"
"<h2>CONTROLS</h2>"
"<div class='sub-section'>"
"<h3>Weapon Controls</h3>"
"<button onclick=\"fetch('/fire').catch(e=>console.log(e))\">FIRE</button>"
"</div>"
"<div class='sub-section'>"
"<h3>Test Mode</h3>"
"<p>Status: <span id='testModeStatus'>DISABLED</span></p>"
"<button id='testModeToggle' onclick='toggleTestMode()'>ENABLE TEST MODE</button>"
"<div id='testControls' class='hidden margin-top'>"
"<p class='test-warning'>WARNING: TEST MODE ACTIVE - Real switches bypassed</p>"
"<button id='testFireButton' onclick='toggleTestFire()'>START FULL AUTO</button>"
"<p>Ammo: <span id='testAmmo'>400</span>/400</p>"
"<div class='margin-top'>"
"<h4>Visual Ammo Counter</h4>"
"<pre id='ammoDisplay' class='ammo-display'>"
"+------------------------------+\n"
"|                              |\n"
"|##############################|\n"
"|##############################|\n"
"|##############################|\n"
"|##############################|\n"
"|##############################|\n"
"|##############################|\n"
"|                              |\n"
"+------------------------------+\n"
"AMMO: 400/400 rounds (10 bars)"
"</pre>"
"</div>"
"</div>"
"</div>"
"<div class='sub-section'>"
"<h3>Camera</h3>"
"<p>Status: <span id='cameraStatus'>Ready</span></p>"
"<div class='camera-controls'>"
"<button onclick='toggleCameraStream()' id='streamToggle'>START STREAM</button>"
"<button onclick='capturePhoto()'>CAPTURE</button>"
"<button onclick='window.open(\"/camera\",\"_blank\")'>FULL PAGE</button>"
"</div>"
"<div id='cameraPreview' class='margin-top'>"
"<div class='camera-preview' id='cameraPlaceholder'>Click START STREAM</div>"
"<img id='cameraStream' class='camera-stream hidden' src='' alt='Camera Stream'>"
"</div>"
"</div>"
"</div>"
"<div class='column'>"
"<h2>NETWORK</h2>"
"<div class='sub-section'>"
"<h3>WiFi Management</h3>"
"<p>Status: <span id='wifiStatus'>Connected</span></p>"
"<p>Network: <span id='connectedSSID'>None</span></p>"
"<p>IP: <span id='currentIP'>192.168.4.1</span></p>"
"<button onclick='scanWiFi()'>SCAN</button>"
"<button onclick='wifi_disconnect()'>DISCONNECT</button>"
"<button onclick='forgetNetwork()'>FORGET</button>"
"<div id='networkList'></div>"
"<div id='connectForm' class='connect-form'>"
"<h3>Connect to: <span id='selectedSSID'></span></h3>"
"<div id='passwordSection'>"
"<label for='wifiPassword'>Password:</label><br>"
"<input type='password' id='wifiPassword' placeholder='Enter WiFi password' class='wifi-password-input'><br>"
"</div>"
"<button onclick='connectToWiFi()' class='margin-top'>CONNECT</button>"
"<button onclick='cancelConnect()' class='margin-top'>CANCEL</button>"
"</div>"
"</div>"
"<div class='sub-section'>"
"<h3>OTA Update</h3>"
"<p>Version: <span id='firmwareVersion'>1.0.0</span></p>"
"<form id='otaForm' enctype='multipart/form-data'>"
"<label for='firmwareFile'>Firmware (.bin):</label><br>"
"<input type='file' id='firmwareFile' name='firmware' accept='.bin' class='file-input'><br>"
"<button type='button' onclick='uploadFirmware()'>UPLOAD</button>"
"</form>"
"<div id='otaProgress' class='hidden margin-top'>"
"<p>Progress: <span id='uploadPercent'>0</span>%</p>"
"<div class='progress-bar-container'>"
"<div id='progressBar' class='progress-bar'></div>"
"</div>"
"</div>"
"</div>"
"</div>"
"</div>"
"</div>"
"<script type='application/javascript' src='/app.js'></script>"
"</body></html>";


// JavaScript Handler
static esp_err_t js_handler(httpd_req_t *req) {
    extern const char app_js_start[] asm("_binary_app_js_start");
    extern const char app_js_end[] asm("_binary_app_js_end");
    const size_t app_js_size = (app_js_end - app_js_start);
    
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    
    return httpd_resp_send(req, app_js_start, app_js_size);
}

// CSS Handler
static esp_err_t css_handler(httpd_req_t *req) {
    extern const char style_css_start[] asm("_binary_style_css_start");
    extern const char style_css_end[] asm("_binary_style_css_end");
    const size_t style_css_size = (style_css_end - style_css_start);
    
    httpd_resp_set_type(req, "text/css");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400"); // Cache for 1 day
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    
    return httpd_resp_send(req, style_css_start, style_css_size);
}

// HTTP Handlers
static esp_err_t index_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Index page requested - URI: %s", req->uri);
    
    // Simple response headers to avoid issues
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    
    // Log User-Agent if available (for debugging large headers)
    size_t user_agent_len = httpd_req_get_hdr_value_len(req, "User-Agent");
    if (user_agent_len > 0 && user_agent_len < 512) { // Limit to reasonable size
        char user_agent[512];
        httpd_req_get_hdr_value_str(req, "User-Agent", user_agent, sizeof(user_agent));
        ESP_LOGI(TAG, "User-Agent length: %zu - %.*s", user_agent_len, 50, user_agent); // Only log first 50 chars
    } else if (user_agent_len > 512) {
        ESP_LOGW(TAG, "User-Agent header too large: %zu bytes", user_agent_len);
    }
    
    return httpd_resp_send(req, index_html, strlen(index_html));
}

// Cached OTA info to reduce frequent calls
static char cached_version[64] = "";
static char cached_partition[256] = "";
static bool ota_info_cached = false;

static esp_err_t status_handler(httpd_req_t *req) {
    char response[1280]; // a bit larger headroom for strings
    char version[64] = "1.0.0";        // Default version
    char partition[256] = "Primary";   // Default partition info

    // Cache OTA info once
    if (!ota_info_cached) {
        if (ota_get_current_version(cached_version, sizeof(cached_version)) != ESP_OK) {
            strcpy(cached_version, "1.0.0-NoOTA");
        }
        if (ota_get_partition_info(cached_partition, sizeof(cached_partition)) != ESP_OK) {
            strcpy(cached_partition, "Boot Partition (No OTA Data)");
        }
        ota_info_cached = true;
        ESP_LOGI(TAG, "OTA info cached - Version: %s, Partition: %s", cached_version, cached_partition);
    }

    // Use cached values
    strncpy(version,   cached_version,   sizeof(version)   - 1);
    strncpy(partition, cached_partition, sizeof(partition) - 1);
    version[sizeof(version)-1]     = '\0';
    partition[sizeof(partition)-1] = '\0';

    // WiFi status string
    const char* wifi_status;
    if (wifi_config_is_connected_to_network()) {
        wifi_status = "Connected to Network";
    } else if (wifi_config_is_ap_active()) {
        wifi_status = "Hotspot Mode";
    } else {
        wifi_status = "Disconnected";
    }

    // Safeguard possibly NULL strings
    const char* ssid = wifi_config_get_connected_ssid();
    if (!ssid) ssid = "";
    const char* ip = wifi_config_get_current_ip();
    if (!ip) ip = "";

    // Show 0 ammo if mag not detected
    int display_ammo = magazine_is_inserted() ? counter_display_get_ammo_count() : 0;

    // Build response
    int n = snprintf(response, sizeof(response),
        "{"
          "\"ammo\":%d,"
          "\"firerate\":%d,"
          "\"volume\":%d,"
          "\"audiomode\":\"%s\","
          "\"flash\":%s,"
          "\"status\":\"Online\","
          "\"version\":\"%s\","
          "\"partition\":\"%s\","
          "\"wifiStatus\":\"%s\","
          "\"connectedSSID\":\"%s\","
          "\"currentIP\":\"%s\","
          "\"torchEnabled\":%s,"
          "\"torchRed\":%d,"
          "\"torchGreen\":%d,"
          "\"torchBlue\":%d,"
          "\"cameraStatus\":\"%s\","
          "\"cameraDisplayEnabled\":%s,"
          "\"bluetoothEnabled\":%s,"
          "\"bluetoothConnected\":%s,"
          "\"testModeEnabled\":%s,"
          "\"magazineInserted\":%s"
        "}",
        display_ammo,
        current_config.fire_rate_rpm,
        current_config.volume,
        current_config.audio_mode == AUDIO_OUTPUT_SPEAKER ? "Speaker" : "Line Out",
        current_config.muzzle_flash_enabled ? "true" : "false",
        version,
        partition,
        wifi_status,
        ssid,
        ip,
        ws2812_torch_get_state() ? "true" : "false",
        current_config.torch_red,
        current_config.torch_green,
        current_config.torch_blue,
        camera_is_initialized() ? "Ready" : "Not Initialized",
        current_config.camera_display_enabled ? "true" : "false",
        bluetooth_transmitter_is_enabled() ? "true" : "false",
        bluetooth_transmitter_is_connected() ? "true" : "false",
        current_config.test_mode_enabled ? "true" : "false",
        magazine_is_inserted() ? "true" : "false"
    );
    if (n < 0 || n >= (int)sizeof(response)) {
        // Truncated (unlikely) – send a minimal safe payload
        strcpy(response, "{\"status\":\"Online\",\"error\":\"resp_truncated\"}");
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}

static esp_err_t config_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Config handler called - Method: %d, Content-Length: %zu", req->method, req->content_len);

    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    size_t hdr_len;
    char hdr_val[128];

    hdr_len = httpd_req_get_hdr_value_len(req, "Content-Type");
    if (hdr_len > 0 && hdr_len < sizeof(hdr_val)) {
        httpd_req_get_hdr_value_str(req, "Content-Type", hdr_val, sizeof(hdr_val));
        ESP_LOGI(TAG, "Content-Type: %s", hdr_val);
    }

    // Receive entire body (guard to 2KB)
    size_t to_read = MIN((size_t)req->content_len, (size_t)2047);
    char *content = (char *)calloc(1, to_read + 1);
    if (!content) {
        ESP_LOGE(TAG, "Out of memory in config_handler");
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "", 0);
    }

    size_t have = 0;
    while (have < to_read) {
        int r = httpd_req_recv(req, content + have, to_read - have);
        if (r <= 0) {
            free(content);
            ESP_LOGE(TAG, "Failed to receive config data, ret=%d", r);
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_set_hdr(req, "Location", "/");
            return httpd_resp_send(req, "", 0);
        }
        have += (size_t)r;
    }
    content[have] = '\0';
    ESP_LOGI(TAG, "Received config data (%u bytes): %s", (unsigned)have, content);

    // URL decode helper function (in place)
    void url_decode_inplace(char *s) {
        char *o = s;
        for (char *i = s; *i; ++i) {
            if (*i == '+') { 
                *o++ = ' '; 
            } else if (*i == '%' && i[1] && i[2]) {
                char hex[3] = { i[1], i[2], 0 };
                *o++ = (char) strtol(hex, NULL, 16);
                i += 2;
            } else {
                *o++ = *i;
            }
        }
        *o = '\0';
    }

    // Tokenize and parse: key=value&key=value
    for (char *tok = strtok(content, "&"); tok; tok = strtok(NULL, "&")) {
        // Each token is key=value
        char *eq = strchr(tok, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = tok;
        char *val = eq + 1;
        url_decode_inplace(key);
        url_decode_inplace(val);

        if (strcmp(key, "volume") == 0) {
            int v = atoi(val);
            if (v < 0) v = 0;
            if (v > 30) v = 30;
            current_config.volume = v;
            dfplayer_set_volume(current_config.volume);
            ESP_LOGI(TAG, "Config: volume=%d", current_config.volume);
        } else if (strcmp(key, "audiomode") == 0) {
            int new_audio_mode = atoi(val);
            if (new_audio_mode != AUDIO_OUTPUT_SPEAKER && new_audio_mode != AUDIO_OUTPUT_LINE_OUT) {
                new_audio_mode = AUDIO_OUTPUT_SPEAKER;
            }
            ESP_LOGI(TAG, "Config change - old audio_mode=%d, new audio_mode=%d",
                     (int)current_config.audio_mode, new_audio_mode);
            current_config.audio_mode = (audio_output_mode_t)new_audio_mode;
            dfplayer_set_audio_mode(current_config.audio_mode);
        } else if (strcmp(key, "camdisplay") == 0) {
            bool enable = (atoi(val) != 0);
            ESP_LOGI(TAG, "Config change - camera display %s to %s",
                     current_config.camera_display_enabled ? "enabled" : "disabled",
                     enable ? "enabled" : "disabled");
            current_config.camera_display_enabled = enable;
        } else if (strcmp(key, "flash") == 0) {
            current_config.muzzle_flash_enabled = (atoi(val) != 0);
        }
    }

    free(content);

    ESP_LOGI(TAG, "Configuration updated: Volume=%d, AudioMode=%d, CamDisplay=%s, Flash=%s",
             current_config.volume,
             (int)current_config.audio_mode,
             current_config.camera_display_enabled ? "ON" : "OFF",
             current_config.muzzle_flash_enabled ? "ON" : "OFF");

    // Apply camera display change
    webserver_update_camera_display();

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, "", 0);
}

static esp_err_t logs_handler(httpd_req_t *req) {
    char logs[2048];

    // Timestamp
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // System info
    size_t free_heap     = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    uint32_t uptime_sec  = esp_timer_get_time() / 1000000U;
    uint32_t uptime_min  = uptime_sec / 60U;
    uint32_t uptime_hr   = uptime_min / 60U;

    // Flash size
    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    // WiFi info, safely handle NULLs
    const char* wifi_status_str;
    if (wifi_config_is_connected_to_network()) {
        wifi_status_str = "Connected to Network";
    } else if (wifi_config_is_ap_active()) {
        wifi_status_str = "Hotspot Mode Active";
    } else {
        wifi_status_str = "WiFi Disconnected";
    }

    const char* connected_ssid = wifi_config_get_connected_ssid();
    if (!connected_ssid) connected_ssid = "";
    const char* current_ip = wifi_config_get_current_ip();
    if (!current_ip) current_ip = "";

    // Ammo reflects mag presence
    int display_ammo = magazine_is_inserted() ? counter_display_get_ammo_count() : 0;
    const char* magazine_status = magazine_is_inserted()
        ? (counter_display_is_empty() ? "Inserted (Empty)" : "Inserted (Loaded)")
        : "Not Detected";

    // Compose HTML
    int n = snprintf(logs, sizeof(logs),
        "[%02d:%02d:%02d] F44AA Pulse Rifle System Status<br>"
        "[INFO] System Uptime: %lu hours, %lu minutes<br>"
        "[INFO] Free Heap: %zu bytes (Min: %zu bytes)<br>"
        "[INFO] Current Ammo: %d/400 rounds<br>"
        "[INFO] Fire Rate: %d RPM<br>"
        "[INFO] Audio Mode: %s (Volume: %d)<br>"
        "[INFO] Muzzle Flash: %s<br>"
        "[INFO] Torch Status: %s RGB(%d,%d,%d)<br>"
        "[INFO] Magazine Status: %s<br>"
        "[INFO] WiFi Status: %s<br>"
        "[INFO] Connected Network: %s<br>"
        "[INFO] Current IP: %s<br>"
        "[INFO] Hotspot SSID: %s<br>"
        "[INFO] Web Server: Running on port 80<br>"
        "[INFO] HTTP Handlers: 16 registered<br>"
        "[INFO] OTA Status: %s<br>"
        "[INFO] System Temperature: Normal<br>"
        "[INFO] Last Config Update: Fire Rate=%d, Volume=%d<br>"
        "[DEBUG] ESP-IDF Version: %s<br>"
        "[DEBUG] Flash Size: %lu MB<br>",
        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
        (unsigned long)uptime_hr, (unsigned long)(uptime_min % 60U),
        free_heap, min_free_heap,
        display_ammo,
        current_config.fire_rate_rpm,
        dfplayer_get_audio_mode() == AUDIO_OUTPUT_SPEAKER ? "Speaker" : "Line Out",
        current_config.volume,
        current_config.muzzle_flash_enabled ? "Enabled" : "Disabled",
        current_config.torch_enabled ? "ON" : "OFF",
        current_config.torch_red, current_config.torch_green, current_config.torch_blue,
        magazine_status,
        wifi_status_str,
        connected_ssid,
        current_ip,
        F44AA_AP_SSID,
        ota_info_cached ? "Ready" : "Initializing",
        current_config.fire_rate_rpm, current_config.volume,
        esp_get_idf_version(),
        (unsigned long)(flash_size / (1024UL * 1024UL))
    );

    if (n < 0 || n >= (int)sizeof(logs)) {
        strcpy(logs, "Logs truncated.\n");
    }

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, logs, strlen(logs));
}

static esp_err_t bluetooth_logs_handler(httpd_req_t *req) {
    char bt_logs[1024];
    
    // Get current time for timestamp
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Get Bluetooth status and information
    bool bt_enabled = bluetooth_transmitter_is_enabled();
    bool bt_connected = bluetooth_transmitter_is_connected();
    
    snprintf(bt_logs, sizeof(bt_logs),
        "[%02d:%02d:%02d] Bluetooth Audio Transmitter Status<br>"
        "[INFO] Bluetooth Module: KCX-BT_EMITTER<br>"
        "[INFO] Status: %s<br>"
        "[INFO] Connection: %s<br>"
        "[INFO] Audio Mode: %s<br>"
        "[INFO] Volume Level: %d<br>"
        "[DEBUG] A2DP Profile: %s<br>"
        "[DEBUG] Audio Codec: SBC<br>"
        "[DEBUG] Sample Rate: 44.1kHz<br>"
        "[DEBUG] Bit Depth: 16-bit<br>"
        "[DEBUG] Audio Source: DFPlayer Pro DAC Output<br>"
        "[DEBUG] Pairing Mode: %s<br>"
        "[DEBUG] Last Connection: %s<br>",
        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
        bt_enabled ? "Enabled" : "Disabled",
        bt_connected ? "Connected to Device" : "Not Connected",
        dfplayer_get_audio_mode() == AUDIO_OUTPUT_SPEAKER ? "Speaker + Bluetooth" : "Line Out + Bluetooth",
        current_config.volume,
        bt_enabled ? "Active" : "Inactive",
        bt_enabled ? "Discoverable" : "Hidden",
        bt_connected ? "Active Device" : "No Device"
    );
    
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, bt_logs, strlen(bt_logs));
}

static esp_err_t bluetooth_logs_clear_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Bluetooth logs cleared via web interface");
    
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "Bluetooth logs cleared", 22);
}

static esp_err_t reload_handler(httpd_req_t *req) {
    if (counter_display_can_reload()) {
        counter_display_reset_ammo();
        ESP_LOGI(TAG, "Magazine reloaded via web interface");
    }
    
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "Reload attempted", 15);
}

static esp_err_t test_audio_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Test audio requested via web interface");
    dfplayer_play_fire();
    
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "Audio test fired", 16);
}

static esp_err_t fire_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Fire requested via web interface");

    // Require a magazine to be inserted (align with existing API used elsewhere)
    if (!magazine_is_inserted()) {
        ESP_LOGW(TAG, "Fire denied - No magazine detected");
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, "Fire denied: No magazine inserted", 33);
    }

    // Trigger muzzle flash if available
    ws2812_flash();

    // Play fire sound
    dfplayer_play_fire();

    // Decrement ammo counter display
    int before_ammo = counter_display_get_ammo_count();
    counter_display_decrement();
    int after_ammo = counter_display_get_ammo_count();

    ESP_LOGI(TAG, "Fire executed - magazine present, LCD counter: %d -> %d", before_ammo, after_ammo);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "Fire executed", 13);
}

static esp_err_t torch_toggle_handler(httpd_req_t *req) {
    current_config.torch_enabled = !current_config.torch_enabled;
    ws2812_torch_set_state(current_config.torch_enabled);
    
    ESP_LOGI(TAG, "Torch toggled %s via web interface", 
             current_config.torch_enabled ? "ON" : "OFF");
    
    char response[64];
    snprintf(response, sizeof(response), "{\"enabled\":%s}", 
             current_config.torch_enabled ? "true" : "false");
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}

static esp_err_t torch_color_handler(httpd_req_t *req) {
    int red = 255, green = 255, blue = 255;
    
    // Check if this is a GET request with query parameters
    if (req->method == HTTP_GET) {
        char query[256];
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
            char param[32];
            if (httpd_query_key_value(query, "r", param, sizeof(param)) == ESP_OK) {
                red = atoi(param);
            }
            if (httpd_query_key_value(query, "g", param, sizeof(param)) == ESP_OK) {
                green = atoi(param);
            }
            if (httpd_query_key_value(query, "b", param, sizeof(param)) == ESP_OK) {
                blue = atoi(param);
            }
        }
    } else {
        // Handle POST request with JSON data (existing functionality)
        char content[256];
        int ret = httpd_req_recv(req, content, sizeof(content) - 1);
        if (ret <= 0) {
            httpd_resp_set_status(req, "400 Bad Request");
            return httpd_resp_send(req, "Failed to receive data", 22);
        }
        content[ret] = '\0';
        
        // Parse JSON: {"red":255,"green":255,"blue":255}
        char *red_pos = strstr(content, "\"red\":");
        char *green_pos = strstr(content, "\"green\":");
        char *blue_pos = strstr(content, "\"blue\":");
        
        if (red_pos) red = atoi(red_pos + 6);
        if (green_pos) green = atoi(green_pos + 8);
        if (blue_pos) blue = atoi(blue_pos + 7);
    }
    
    // Clamp values to 0-255
    red = (red < 0) ? 0 : (red > 255) ? 255 : red;
    green = (green < 0) ? 0 : (green > 255) ? 255 : green;
    blue = (blue < 0) ? 0 : (blue > 255) ? 255 : blue;
    
    current_config.torch_red = red;
    current_config.torch_green = green;
    current_config.torch_blue = blue;
    
    ws2812_torch_set_color(red, green, blue);
    
    ESP_LOGI(TAG, "Torch color set to RGB(%d,%d,%d) via web interface", red, green, blue);
    
    httpd_resp_set_type(req, "application/json");
    char response[128];
    snprintf(response, sizeof(response), "{\"red\":%d,\"green\":%d,\"blue\":%d,\"status\":\"success\"}", red, green, blue);
    return httpd_resp_send(req, response, strlen(response));
}

static esp_err_t test_mode_toggle_handler(httpd_req_t *req) {
    current_config.test_mode_enabled = !current_config.test_mode_enabled;

    ESP_LOGI(TAG, "Test mode %s via web interface",
             current_config.test_mode_enabled ? "ENABLED" : "DISABLED");

    if (current_config.test_mode_enabled) {
        // Enable test mode - override sensors and set full ammo
        magazine_set_override(true);             // Force "inserted"
        counter_display_force_reset_ammo();      // Reset to full (400)
        ESP_LOGI(TAG, "Test mode enabled - magazine override active, ammo reset to 400");
    } else {
        // Disable test mode - return to real sensor readings and clear display
        magazine_set_override(false);            // Stop overriding
        counter_display_set_empty();             // Clear counter display (no magazine)
        ESP_LOGI(TAG, "Test mode disabled - returned to real sensor readings, display cleared");
    }

    httpd_resp_set_type(req, "application/json");
    char response[128];
    snprintf(response, sizeof(response),
             "{\"enabled\":%s,\"status\":\"success\",\"ammo\":%d}",
             current_config.test_mode_enabled ? "true" : "false",
             current_config.test_mode_enabled ? 400 : 0);
    return httpd_resp_send(req, response, strlen(response));
}

static esp_err_t test_fire_start_handler(httpd_req_t *req) {
    if (!current_config.test_mode_enabled) {
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"Test mode not enabled\"}", 31);
    }
    
    // Check if we have ammo
    int current_ammo = counter_display_get_ammo_count();
    if (current_ammo <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"No ammo remaining\"}", 29);
    }
    
    // Create timer if it doesn't exist
    if (test_fire_timer == NULL) {
        esp_timer_create_args_t timer_args = {
            .callback = test_fire_timer_callback,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "test_fire_timer"
        };
        esp_err_t ret = esp_timer_create(&timer_args, &test_fire_timer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create test fire timer: %s", esp_err_to_name(ret));
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_set_type(req, "application/json");
            return httpd_resp_send(req, "{\"error\":\"Timer creation failed\"}", 33);
        }
    }
    
    // Start continuous firing at 900 RPM (66.67ms intervals)
    test_firing_active = true;
    esp_err_t ret = esp_timer_start_periodic(test_fire_timer, 66670); // 66.67ms in microseconds
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start test fire timer: %s", esp_err_to_name(ret));
        test_firing_active = false;
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"Timer start failed\"}", 31);
    }
    
    ESP_LOGI(TAG, "Test fire started - 900 RPM continuous firing");
    
    httpd_resp_set_type(req, "application/json");
    char response[128];
    snprintf(response, sizeof(response), "{\"status\":\"firing_started\",\"ammo\":%d}", current_ammo);
    return httpd_resp_send(req, response, strlen(response));
}

static esp_err_t test_fire_stop_handler(httpd_req_t *req) {
    if (!current_config.test_mode_enabled) {
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"Test mode not enabled\"}", 31);
    }
    
    // Stop the timer
    test_firing_active = false;
    if (test_fire_timer != NULL) {
        esp_err_t ret = esp_timer_stop(test_fire_timer);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Failed to stop test fire timer: %s", esp_err_to_name(ret));
        }
    }
    
    ESP_LOGI(TAG, "Test fire stopped via web interface");
    
    int current_ammo = counter_display_get_ammo_count();
    
    httpd_resp_set_type(req, "application/json");
    char response[128];
    snprintf(response, sizeof(response), "{\"status\":\"firing_stopped\",\"ammo\":%d}", current_ammo);
    return httpd_resp_send(req, response, strlen(response));
}

static esp_err_t config_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Config GET handler called - URI: %s", req->uri);
    
    // Parse query parameters from URI: /config-get?volume=25&audiomode=0
    char query[256];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        ESP_LOGI(TAG, "Query string: %s", query);
        
        char param[32];
        
        // Parse volume
        if (httpd_query_key_value(query, "volume", param, sizeof(param)) == ESP_OK) {
            current_config.volume = atoi(param);
            dfplayer_set_volume(current_config.volume);
            ESP_LOGI(TAG, "Updated volume to: %d", current_config.volume);
        }
        
        // Parse audio mode
        if (httpd_query_key_value(query, "audiomode", param, sizeof(param)) == ESP_OK) {
            int new_audio_mode = atoi(param);
            ESP_LOGI(TAG, "Updating audio mode from %d to %d", current_config.audio_mode, new_audio_mode);
            current_config.audio_mode = new_audio_mode;
            dfplayer_set_audio_mode(current_config.audio_mode);
            ESP_LOGI(TAG, "Audio mode updated successfully");
        }
    }
    
    // Redirect back to main page
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_set_hdr(req, "Connection", "close");
    return httpd_resp_send(req, "", 0);
}

// Simple test handler for debugging HTTP issues
static esp_err_t test_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Test handler called");
    
    const char* test_response = 
        "<html><head><link rel='stylesheet' href='/style.css'></head><body class='test-page-body'>"
        "<h1 class='test-page-title'>F44AA TEST PAGE</h1>"
        "<div class='test-page-content'>"
        "<p>If you can see this, the HTTP server is working.</p>"
        "<p><a href='/' class='test-page-link'>Go to Main Page</a></p>"
        "</div></body></html>";
    
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, test_response, strlen(test_response));
}

// Counter display color handler
static esp_err_t display_color_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Display color handler called");
    
    // Parse color parameter from query string
    char query[64];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char color_str[8];
        if (httpd_query_key_value(query, "color", color_str, sizeof(color_str)) == ESP_OK) {
            // Parse color value as hex
            uint16_t color_value = (uint16_t)strtol(color_str, NULL, 16);
            
            // Update the display color
            counter_display_set_bar_color(color_value);
            
            ESP_LOGI(TAG, "Display color set to 0x%04X", color_value);
            
            char response[64];
            snprintf(response, sizeof(response), "{\"color\":\"0x%04X\"}", color_value);
            
            httpd_resp_set_type(req, "application/json");
            return httpd_resp_send(req, response, strlen(response));
        }
    }
    
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "Invalid color parameter", 23);
}

// HTTP Error Handler for 431 and other HTTP errors
static esp_err_t http_error_handler(httpd_req_t *req, httpd_err_code_t err) {
    ESP_LOGW(TAG, "HTTP Error Handler called - Error: %d", err);
    
    // Check if this is a form submission that failed
    bool is_config_request = (req && req->uri[0] != '\0' && strstr(req->uri, "/config") != NULL);
    
    // Handle different types of HTTP errors
    switch (err) {
        case HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE:
        case HTTPD_400_BAD_REQUEST:
            ESP_LOGW(TAG, "HTTP %d - Request too large or malformed, sending simplified response", err);
            
            // Try to send a simple response using the standard HTTP response functions first
            esp_err_t resp_err = ESP_FAIL;
            
            // Attempt 1: Use standard HTTP response (safest)
            resp_err = httpd_resp_set_type(req, "text/html");
            if (resp_err == ESP_OK) {
                resp_err = httpd_resp_set_hdr(req, "Connection", "close");
            }
            if (resp_err == ESP_OK) {
                const char* simple_html;
                
                // Provide different response based on request type
                if (is_config_request) {
                    simple_html = 
                        "<html><head><link rel='stylesheet' href='/style.css'></head><body class='error-page-body'>"
                        "<h1>F44AA PULSE RIFLE</h1>"
                        "<p>Configuration update failed - request too large.</p>"
                        "<p><a href='/'>Return to Main Page</a></p>"
                        "<script>setTimeout(function(){window.location.href='/';}, 3000);</script>"
                        "</body></html>";
                } else {
                    simple_html = 
                        "<html><head><link rel='stylesheet' href='/style.css'></head><body class='error-page-body'>"
                        "<h1>F44AA PULSE RIFLE</h1>"
                        "<p>Request too large. <a href='/'>Click to reload</a></p>"
                        "</body></html>";
                }
                
                resp_err = httpd_resp_send(req, simple_html, strlen(simple_html));
            }
            
            // Attempt 2: If standard method fails, try raw socket (fallback)
            if (resp_err != ESP_OK) {
                ESP_LOGW(TAG, "Standard HTTP response failed, trying raw socket");
                int sock = httpd_req_to_sockfd(req);
                if (sock >= 0) {
                    const char* raw_response;
                    
                    if (is_config_request) {
                        raw_response = 
                            "HTTP/1.1 302 Found\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 200\r\n"
                            "Location: /\r\n"
                            "Connection: close\r\n"
                            "Cache-Control: no-cache\r\n"
                            "\r\n"
                            "<html><head><link rel='stylesheet' href='/style.css'></head><body class='error-page-body'>"
                            "<h1>F44AA</h1><p>Config failed - redirecting...</p>"
                            "<script>window.location.href='/';</script></body></html>";
                    } else {
                        raw_response = 
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 150\r\n"
                            "Connection: close\r\n"
                            "Cache-Control: no-cache\r\n"
                            "\r\n"
                            "<html><head><link rel='stylesheet' href='/style.css'></head><body class='error-page-body'>"
                            "<h1>F44AA</h1><p><a href='/'>Reload</a></p></body></html>";
                    }
                    
                    ssize_t sent = send(sock, raw_response, strlen(raw_response), 0);
                    ESP_LOGI(TAG, "Raw socket send result: %d bytes", (int)sent);
                }
            }
            
            return ESP_OK;
            
        default:
            ESP_LOGW(TAG, "Unhandled HTTP error: %d", err);
            return ESP_FAIL;
    }
}

// Initialize HTTP Server
static esp_err_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_uri_handlers = 30;  // Increased to 30 handlers to accommodate test mode handlers
    config.stack_size = 12288;     // Increased to 12KB to handle large headers
    config.max_resp_headers = 16;  // Increased back to 16 response headers
    config.recv_wait_timeout = 15; // Increased to 15 seconds for large requests
    config.send_wait_timeout = 15; // Increased to 15 seconds
    config.uri_match_fn = httpd_uri_match_wildcard; // Allow better URI matching
    config.task_priority = 5;      // Set HTTP server task priority higher than trigger task (4)
    
    // Set backlog queue for pending connections
    config.backlog_conn = 10;
    
    // Additional robustness settings
    config.close_fn = NULL;
    config.global_user_ctx = NULL;
    config.global_user_ctx_free_fn = NULL;
    config.open_fn = NULL;

    ESP_LOGI(TAG, "Starting HTTP server on port: '%d' with %d handler slots", 
             config.server_port, config.max_uri_handlers);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");
        
        // Register error handlers for HTTP errors
        httpd_register_err_handler(server, HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE, http_error_handler);
        httpd_register_err_handler(server, HTTPD_400_BAD_REQUEST, http_error_handler);
        ESP_LOGI(TAG, "HTTP error handlers registered for 400 and 431 errors");
        
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler
        };
        httpd_register_uri_handler(server, &index_uri);

        // Register CSS handler for external stylesheet
        httpd_uri_t css_uri = {
            .uri = "/style.css",
            .method = HTTP_GET,
            .handler = css_handler
        };
        httpd_register_uri_handler(server, &css_uri);

        // Register JS handler for external JavaScript
        httpd_uri_t js_uri = {
            .uri = "/app.js",
            .method = HTTP_GET,
            .handler = js_handler
        };
        httpd_register_uri_handler(server, &js_uri);

        httpd_uri_t camera_page_uri = {
            .uri = "/camera",
            .method = HTTP_GET,
            .handler = webserver_camera_page_handler
        };
        httpd_register_uri_handler(server, &camera_page_uri);

        httpd_uri_t status_uri = {
            .uri = "/status",
            .method = HTTP_GET,
            .handler = status_handler
        };
        httpd_register_uri_handler(server, &status_uri);

        httpd_uri_t config_uri = {
            .uri = "/config",
            .method = HTTP_POST,
            .handler = config_handler
        };
        httpd_register_uri_handler(server, &config_uri);

        // Add a GET-based config handler as a fallback for problematic POST requests
        httpd_uri_t config_get_uri = {
            .uri = "/config-get",
            .method = HTTP_GET,
            .handler = config_get_handler
        };
        httpd_register_uri_handler(server, &config_get_uri);

        httpd_uri_t logs_uri = {
            .uri = "/logs",
            .method = HTTP_GET,
            .handler = logs_handler
        };
        httpd_register_uri_handler(server, &logs_uri);

        httpd_uri_t bluetooth_logs_uri = {
            .uri = "/bluetooth-logs",
            .method = HTTP_GET,
            .handler = bluetooth_logs_handler
        };
        httpd_register_uri_handler(server, &bluetooth_logs_uri);

        // Allow both GET and POST to match the front-end fetch() default
        httpd_uri_t bluetooth_logs_clear_get_uri = {
            .uri = "/bluetooth-logs/clear",
            .method = HTTP_GET,
            .handler = bluetooth_logs_clear_handler
        };
        httpd_register_uri_handler(server, &bluetooth_logs_clear_get_uri);

        httpd_uri_t bluetooth_logs_clear_post_uri = {
            .uri = "/bluetooth-logs/clear",
            .method = HTTP_POST,
            .handler = bluetooth_logs_clear_handler
        };
        httpd_register_uri_handler(server, &bluetooth_logs_clear_post_uri);

        httpd_uri_t reload_uri = {
            .uri = "/reload",
            .method = HTTP_GET,
            .handler = reload_handler
        };
        httpd_register_uri_handler(server, &reload_uri);

        httpd_uri_t test_audio_uri = {
            .uri = "/test-audio",
            .method = HTTP_GET,
            .handler = test_audio_handler
        };
        httpd_register_uri_handler(server, &test_audio_uri);

        httpd_uri_t fire_uri = {
            .uri = "/fire",
            .method = HTTP_GET,
            .handler = fire_handler
        };
        httpd_register_uri_handler(server, &fire_uri);

        httpd_uri_t torch_toggle_uri = {
            .uri = "/torch-toggle",
            .method = HTTP_GET,
            .handler = torch_toggle_handler
        };
        httpd_register_uri_handler(server, &torch_toggle_uri);

        httpd_uri_t torch_color_uri = {
            .uri = "/torch-color",
            .method = HTTP_GET,
            .handler = torch_color_handler
        };
        httpd_register_uri_handler(server, &torch_color_uri);

        httpd_uri_t torch_color_post_uri = {
            .uri = "/torch-color",
            .method = HTTP_POST,
            .handler = torch_color_handler
        };
        httpd_register_uri_handler(server, &torch_color_post_uri);

        // Register OTA handlers via module
        webserver_ota_register_handlers(server);

        // Register WiFi handlers via module  
        ESP_LOGI(TAG, "Registering WiFi handlers via module");
        webserver_wifi_register_handlers(server);

        // Register test handler for debugging
        httpd_uri_t test_uri = {
            .uri = "/test",
            .method = HTTP_GET,
            .handler = test_handler
        };
        httpd_register_uri_handler(server, &test_uri);

        // Register camera handlers via module
        webserver_camera_register_handlers(server);

        httpd_uri_t display_color_uri = {
            .uri = "/display-color",
            .method = HTTP_GET,
            .handler = display_color_handler
        };
        httpd_register_uri_handler(server, &display_color_uri);

        // Test mode handlers
        httpd_uri_t test_mode_toggle_uri = {
            .uri = "/test-mode/toggle",
            .method = HTTP_GET,
            .handler = test_mode_toggle_handler
        };
        esp_err_t ret = httpd_register_uri_handler(server, &test_mode_toggle_uri);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register test_mode_toggle_uri: %s", esp_err_to_name(ret));
        }

        httpd_uri_t test_fire_start_uri = {
            .uri = "/test-mode/fire-start",
            .method = HTTP_GET,
            .handler = test_fire_start_handler
        };
        ret = httpd_register_uri_handler(server, &test_fire_start_uri);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register test_fire_start_uri: %s", esp_err_to_name(ret));
        }

        httpd_uri_t test_fire_stop_uri = {
            .uri = "/test-mode/fire-stop",
            .method = HTTP_GET,
            .handler = test_fire_stop_handler
        };
        ret = httpd_register_uri_handler(server, &test_fire_stop_uri);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register test_fire_stop_uri: %s", esp_err_to_name(ret));
        }

        ESP_LOGI(TAG, "Test mode handlers registered successfully");
        ESP_LOGI(TAG, "Camera handlers registered successfully");

        server_running = true;
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Error starting server!");
    return ESP_FAIL;
}

// Public Functions
webserver_err_t webserver_init(void) {
    ESP_LOGI(TAG, "Initializing F44AA web server with WiFi configuration");

    // Initialize WiFi configuration system
    wifi_config_init();
    wifi_config_init_ap();
    
    // Try to auto-connect to saved WiFi network
    esp_err_t wifi_ret = wifi_config_try_auto_connect();
    if (wifi_ret == ESP_OK) {
        ESP_LOGI(TAG, "Successfully connected to saved WiFi network");
    } else {
        ESP_LOGI(TAG, "No saved WiFi credentials or connection failed, using AP mode");
    }
    
    // Initialize torch with default color
    ws2812_torch_set_color(current_config.torch_red, current_config.torch_green, current_config.torch_blue);
    ws2812_torch_set_state(current_config.torch_enabled);
    
    esp_err_t webserver_ret = start_webserver();
    if (webserver_ret == ESP_OK) {
        return WEBSERVER_OK;
    } else {
        return WEBSERVER_INIT_FAIL;
    }
}

void webserver_stop(void) {
    // Clean up test fire timer
    if (test_fire_timer != NULL) {
        test_firing_active = false;
        esp_timer_stop(test_fire_timer);
        esp_timer_delete(test_fire_timer);
        test_fire_timer = NULL;
        ESP_LOGI(TAG, "Test fire timer cleaned up");
    }
    
    if (server) {
        httpd_stop(server);
        server = NULL;
        server_running = false;
        ESP_LOGI(TAG, "Web server stopped");
    }
}

bool webserver_is_running(void) {
    return server_running;
}

const char* webserver_get_ip(void) {
    return wifi_config_get_current_ip();
}

bool webserver_get_camera_display_enabled(void) {
    return current_config.camera_display_enabled;
}

// Forward declarations for camera display management (implemented in main.c)
extern void camera_display_start(void);
extern void camera_display_stop(void);
extern TaskHandle_t camera_display_task_handle;

void webserver_update_camera_display(void) {
    if (current_config.camera_display_enabled) {
        // Enable camera display
        if (camera_display_task_handle == NULL) {
            ESP_LOGI(TAG, "Enabling camera display via web interface");
            camera_display_start();
        }
    } else {
        // Disable camera display
        if (camera_display_task_handle != NULL) {
            ESP_LOGI(TAG, "Disabling camera display via web interface");
            camera_display_stop();
        }
    }
}