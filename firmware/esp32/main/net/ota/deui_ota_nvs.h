#pragma once

#include "esp_err.h"

esp_err_t deui_ota_nvs_save_last_version(const char *version);
esp_err_t deui_ota_nvs_save_last_error(const char *message);
