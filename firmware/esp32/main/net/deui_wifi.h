#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef struct {
  bool ap_running;
  bool sta_connected;
  bool sta_connecting;
  bool has_saved_credentials;
  char ap_ssid[33];
  char ap_password[65];
  char sta_ip[16];
  char hostname[33];
} deui_wifi_info_t;

esp_err_t deui_wifi_init(void);
esp_err_t deui_wifi_suspend(void);
esp_err_t deui_wifi_resume(void);
bool deui_wifi_is_suspended(void);
bool deui_wifi_is_provisioning(void);
/** True when at least one station is associated with the setup AP. */
bool deui_wifi_ap_has_station(void);
/** True during an active phone join/DHCP window (BLE yields briefly). */
bool deui_wifi_join_boost_active(void);
bool deui_wifi_block_ble_scan(void);
/** Fallback if Wi-Fi events are missed: detect associated STAs via esp_wifi_ap_get_sta_list. */
void deui_wifi_poll_ap_clients(void);
/** Extend the Wi-Fi-priority window (microseconds). */
void deui_wifi_extend_join_boost(int64_t duration_us, const char *reason);
void deui_wifi_get_info(deui_wifi_info_t *info);
