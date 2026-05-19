#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "deui_wifi.h"

#define DEUI_WIFI_PORTAL_IP "192.168.4.1"
#define DEUI_WIFI_PORTAL_URL "http://deui.local/"
#define DEUI_WIFI_PORTAL_PASS "deui-setup"
#define DEUI_WIFI_DEFAULT_HOSTNAME "deui"

#define DEUI_WIFI_NVS_NAMESPACE "deui_wifi"
#define DEUI_WIFI_NVS_KEY_SSID "ssid"
#define DEUI_WIFI_NVS_KEY_PASS "pass"
#define DEUI_WIFI_NVS_KEY_HOST "host"

typedef struct {
  deui_wifi_info_t info;
  bool initialized;
  bool suspended;
  bool portal_running;
  bool dns_running;
  char sta_ssid[33];
  char sta_password[65];
  httpd_handle_t httpd;
  TaskHandle_t dns_task;
  int dns_socket;
  esp_timer_handle_t reconnect_timer;
  esp_timer_handle_t setup_teardown_timer;
  esp_timer_handle_t setup_restore_timer;
  bool sta_provision_active;
  bool sta_provision_failed;
  int64_t sta_provision_started_us;
  uint8_t sta_last_disconnect_reason;
  uint8_t sta_disconnect_count;
  uint8_t ap_station_count;
  bool ap_awaiting_dhcp;
  bool ap_dhcp_done;
  int64_t wifi_join_boost_until_us;
  uint32_t sta_retry_delay_ms;
  esp_netif_t *sta_netif;
  esp_netif_t *ap_netif;
} deui_wifi_state_t;

extern deui_wifi_state_t g_deui_wifi;

void deui_wifi_fill_ap_ssid(char *out, size_t out_size);
esp_err_t deui_wifi_ensure_sta_netif(void);
esp_err_t deui_wifi_configure_ap(bool apsta_mode);
esp_err_t deui_wifi_disable_ap(void);
void deui_wifi_log_running_ap_config(void);

esp_err_t deui_wifi_settings_load(char *ssid, size_t ssid_size, char *password, size_t password_size, char *hostname,
                                  size_t hostname_size);
esp_err_t deui_wifi_settings_save(const char *ssid, const char *password, const char *hostname);
esp_err_t deui_wifi_settings_clear(void);

esp_err_t deui_wifi_start_portal(void);
/** Start the HTTP portal on STA when credentials are saved (home Wi-Fi). */
esp_err_t deui_wifi_portal_ensure_on_sta(void);
esp_err_t deui_wifi_stop_portal(void);
esp_err_t deui_wifi_portal_register_handlers(httpd_handle_t server);

esp_err_t deui_wifi_start_captive_dns(void);
void deui_wifi_stop_captive_dns(void);

esp_err_t deui_wifi_apply_station_config(bool keep_ap_running);
esp_err_t deui_wifi_connect_station(void);
void deui_wifi_note_got_ip(const ip_event_got_ip_t *event);
void deui_wifi_handle_sta_disconnect(void);

void deui_wifi_provision_begin(void);
void deui_wifi_provision_on_got_ip(void);
void deui_wifi_provision_note_sta_disconnect(uint8_t reason);
void deui_wifi_provision_write_status_json(char *buf, size_t buf_size);
/** After HTTP response completes, restore setup SoftAP (network reset). */
void deui_wifi_schedule_restore_setup_ap(void);

uint32_t deui_wifi_reconnect_delay_ms(uint8_t disconnect_count);
bool deui_wifi_reopen_ap_after_disconnects(uint8_t disconnect_count);

esp_err_t deui_wifi_register_events(void);

void deui_wifi_apply_coex_preference(void);
void deui_wifi_extend_join_boost(int64_t duration_us, const char *reason);
bool deui_wifi_join_boost_active(void);
bool deui_wifi_ap_has_station(void);
bool deui_wifi_ap_awaiting_dhcp(void);
/** Call when a phone has associated (event or driver poll); holds radio for DHCP. */
void deui_wifi_note_ap_client_associated(const char *reason);
void deui_wifi_note_ap_client_gone(const char *reason);
/** True while BLE discovery must stay off (join boost or DHCP in progress). */
bool deui_wifi_block_ble_scan(void);
void deui_wifi_poll_ap_clients(void);

esp_err_t deui_wifi_mdns_init(void);
esp_err_t deui_wifi_mdns_apply_hostname(void);
void deui_wifi_mdns_on_got_ip(void);
