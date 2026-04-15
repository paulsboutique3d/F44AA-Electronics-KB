# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

set(COMPONENT_SRCS 
    "main.c"
    "ai_targeting.c"  # AI-powered targeting system integration
    "bluetooth_transmitter.c"  # KCX-BT_EMITTER Bluetooth audio transmitter
    "camera_esp32s3.c"  # Real ESP32-S3-CAM hardware implementation
    "camera_display.c"  # Second IPS screen for camera output
    "color_calibration.c"  # Color calibration system
    "counter_display.c"
    "dfplayer.c"
    "led_strip_encoder.c"  # LED strip encoder for new RMT driver
    "magazine.c"
    "ota_update.c"
    "person_detection.cpp"  # ESP-DL person detection implementation
    "trigger.c"
    "webserver.c"
    "wifi_config.c"
    "ws2812.c"
    # Webserver modules
    "webserver/webserver_ota.c"
    "webserver/webserver_camera.c"
    "webserver/webserver_wifi.c"
)

set(COMPONENT_ADD_INCLUDEDIRS "../include" "webserver")

idf_component_register(SRCS ${COMPONENT_SRCS}
                       INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
                       PRIV_REQUIRES esp_driver_rmt nvs_flash driver esp_wifi esp_netif esp_http_server esp_common freertos app_update esp_timer esp32-camera spi_flash espressif__esp-dl espressif__pedestrian_detect)

# Embed the CSS file as a binary
target_add_binary_data(${COMPONENT_LIB} "../data/style.css" TEXT)
