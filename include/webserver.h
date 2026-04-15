/**
 * @file webserver.h
 * @brief F44AA Web Interface Server
 * 
 * HTTP server for configuration and control via web browser.
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Web server status
typedef enum {
    WEBSERVER_OK = 0,
    WEBSERVER_INIT_FAIL = -1,
    WEBSERVER_NOT_RUNNING = -2,
    WEBSERVER_CAMERA_ERROR = -3
} webserver_err_t;

// Web server functions
webserver_err_t webserver_init(void);
void webserver_stop(void);
bool webserver_is_running(void);
const char* webserver_get_ip(void);
void webserver_handle_clients(void);

// Configuration functions
bool webserver_get_camera_display_enabled(void);
void webserver_update_camera_display(void);

// Camera streaming functions
void webserver_handle_stream(void);
void webserver_handle_capture(void);
void webserver_handle_status(void);

#ifdef __cplusplus
}
#endif
