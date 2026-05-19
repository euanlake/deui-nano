#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t deui_wifi_portal_register_asset_handlers(httpd_handle_t server);
