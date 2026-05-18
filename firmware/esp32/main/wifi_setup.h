#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef struct {
  bool ap_running;
  bool sta_connected;
  char ap_ssid[33];
  char hostname[33];
} deui_wifi_info_t;

esp_err_t deui_wifi_init(void);
esp_err_t deui_wifi_suspend(void);
esp_err_t deui_wifi_resume(void);
bool deui_wifi_is_suspended(void);
void deui_wifi_get_info(deui_wifi_info_t *info);
