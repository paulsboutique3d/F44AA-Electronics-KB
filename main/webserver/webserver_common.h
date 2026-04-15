#ifndef WEBSERVER_COMMON_H
#define WEBSERVER_COMMON_H

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include <string.h>
#include <stdlib.h>

// Common HTTP response codes
#define HTTP_200_OK "200 OK"
#define HTTP_302_FOUND "302 Found"
#define HTTP_400_BAD_REQUEST "400 Bad Request"
#define HTTP_404_NOT_FOUND "404 Not Found"
#define HTTP_500_INTERNAL_ERROR "500 Internal Server Error"
#define HTTP_503_SERVICE_UNAVAILABLE "503 Service Unavailable"

// Common content types
#define CONTENT_TYPE_JSON "application/json"
#define CONTENT_TYPE_HTML "text/html"
#define CONTENT_TYPE_PLAIN "text/plain"
#define CONTENT_TYPE_OCTET_STREAM "application/octet-stream"

// Common response helpers
#define WEBSERVER_SEND_JSON_RESPONSE(req, json_str) do { \
    httpd_resp_set_type(req, CONTENT_TYPE_JSON); \
    return httpd_resp_send(req, json_str, strlen(json_str)); \
} while(0)

#define WEBSERVER_SEND_ERROR(req, status, message) do { \
    httpd_resp_set_status(req, status); \
    httpd_resp_set_type(req, CONTENT_TYPE_PLAIN); \
    return httpd_resp_send(req, message, strlen(message)); \
} while(0)

#define WEBSERVER_REDIRECT(req, location) do { \
    httpd_resp_set_status(req, HTTP_302_FOUND); \
    httpd_resp_set_hdr(req, "Location", location); \
    httpd_resp_set_hdr(req, "Connection", "close"); \
    return httpd_resp_send(req, "", 0); \
} while(0)

// Buffer size constants
#define WEBSERVER_SMALL_BUFFER 256
#define WEBSERVER_MEDIUM_BUFFER 512
#define WEBSERVER_LARGE_BUFFER 1024
#define WEBSERVER_HUGE_BUFFER 2048

// Common utilities
static inline void webserver_set_cors_headers(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

#endif // WEBSERVER_COMMON_H
