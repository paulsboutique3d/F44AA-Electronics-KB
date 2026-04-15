#ifndef WEBSERVER_OTA_H
#define WEBSERVER_OTA_H

#include "webserver_common.h"
#include "ota_update.h"

// OTA HTTP handlers
esp_err_t webserver_ota_upload_handler(httpd_req_t *req);
esp_err_t webserver_ota_status_handler(httpd_req_t *req);

// Register OTA handlers with the web server
void webserver_ota_register_handlers(httpd_handle_t server);

#endif // WEBSERVER_OTA_H
