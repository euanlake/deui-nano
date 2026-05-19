#pragma once

#include <stdint.h>

#include "esp_wifi_types.h"

const char *deui_wifi_authmode_name(wifi_auth_mode_t mode);
const char *deui_wifi_reason_name(uint16_t reason);
const char *deui_wifi_event_name(int32_t event_id);
void deui_wifi_enable_verbose_logging(void);
/** Log AP join state (boost window, clients, portal) for serial monitor diagnosis. */
void deui_wifi_log_join_state(const char *phase);
