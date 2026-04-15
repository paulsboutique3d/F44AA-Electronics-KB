#ifndef WEBSERVER_CAMERA_H
#define WEBSERVER_CAMERA_H

#include "webserver_common.h"
#include "camera.h"
#include "img_converters.h"

// Camera HTTP handlers
esp_err_t webserver_camera_stream_handler(httpd_req_t *req);
esp_err_t webserver_camera_page_handler(httpd_req_t *req);
esp_err_t webserver_camera_capture_handler(httpd_req_t *req);
esp_err_t webserver_camera_quality_handler(httpd_req_t *req);

// Register camera handlers with the web server
void webserver_camera_register_handlers(httpd_handle_t server);

// Camera utility functions
esp_err_t webserver_camera_send_test_image(httpd_req_t *req);

#endif // WEBSERVER_CAMERA_H
