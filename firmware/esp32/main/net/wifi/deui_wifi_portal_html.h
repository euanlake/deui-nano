#pragma once

#include "esp_http_server.h"
#include "esp_wifi.h"

esp_err_t deui_wifi_portal_send_home(httpd_req_t *req);
esp_err_t deui_wifi_portal_send_wifi(httpd_req_t *req, const char *prefill_ssid);
esp_err_t deui_wifi_portal_send_weight(httpd_req_t *req);
esp_err_t deui_wifi_portal_send_message(httpd_req_t *req, const char *title, const char *message);
esp_err_t deui_wifi_portal_send_connecting(httpd_req_t *req, const char *ssid);
esp_err_t deui_wifi_portal_send_scan(httpd_req_t *req, const wifi_ap_record_t *records, uint16_t count);
