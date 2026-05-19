#pragma once

#include "esp_http_server.h"
#include "esp_err.h"

esp_err_t deui_wifi_portal_register_font_handlers(httpd_handle_t server);
