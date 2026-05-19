#include "deui_wifi_internal.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "lwip/inet.h"

static const char *TAG = "deui_wifi_sta";

esp_err_t deui_wifi_apply_station_config(bool keep_ap_running) {
  wifi_config_t sta_cfg = {0};
  wifi_mode_t mode = keep_ap_running ? WIFI_MODE_APSTA : WIFI_MODE_STA;

  if (g_deui_wifi.sta_ssid[0] == '\0') {
    return ESP_ERR_INVALID_STATE;
  }

  ESP_RETURN_ON_ERROR(deui_wifi_ensure_sta_netif(), TAG, "STA netif create failed");

  strlcpy((char *)sta_cfg.sta.ssid, g_deui_wifi.sta_ssid, sizeof(sta_cfg.sta.ssid));
  strlcpy((char *)sta_cfg.sta.password, g_deui_wifi.sta_password, sizeof(sta_cfg.sta.password));
  sta_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
  sta_cfg.sta.pmf_cfg.capable = true;
  sta_cfg.sta.pmf_cfg.required = false;

  ESP_RETURN_ON_ERROR(esp_wifi_set_mode(mode), TAG, "Set STA mode failed");
  ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg), TAG, "Set STA config failed");
  if (g_deui_wifi.sta_netif != NULL) {
    (void)esp_netif_set_hostname(g_deui_wifi.sta_netif, g_deui_wifi.info.hostname);
  }
  (void)deui_wifi_mdns_apply_hostname();
  return ESP_OK;
}

esp_err_t deui_wifi_connect_station(void) {
  esp_err_t err = esp_wifi_connect();
  if (err == ESP_OK) {
    g_deui_wifi.info.sta_connecting = true;
  }
  return err;
}

void deui_wifi_note_got_ip(const ip_event_got_ip_t *event) {
  if (event == NULL) {
    return;
  }
  g_deui_wifi.info.sta_connected = true;
  g_deui_wifi.info.sta_connecting = false;
  g_deui_wifi.sta_disconnect_count = 0;
  g_deui_wifi.sta_retry_delay_ms = 0;
  if (g_deui_wifi.reconnect_timer != NULL) {
    (void)esp_timer_stop(g_deui_wifi.reconnect_timer);
  }
  (void)snprintf(g_deui_wifi.info.sta_ip, sizeof(g_deui_wifi.info.sta_ip), IPSTR, IP2STR(&event->ip_info.ip));
}

void deui_wifi_handle_sta_disconnect(void) {
  uint32_t delay_ms;
  bool reopen_ap;

  g_deui_wifi.info.sta_connected = false;
  g_deui_wifi.info.sta_connecting = g_deui_wifi.info.has_saved_credentials;
  g_deui_wifi.info.sta_ip[0] = '\0';
  if (!g_deui_wifi.info.has_saved_credentials) {
    return;
  }

  g_deui_wifi.sta_disconnect_count++;
  deui_wifi_provision_note_sta_disconnect(g_deui_wifi.sta_last_disconnect_reason);
  delay_ms = deui_wifi_reconnect_delay_ms(g_deui_wifi.sta_disconnect_count);
  reopen_ap = deui_wifi_reopen_ap_after_disconnects(g_deui_wifi.sta_disconnect_count);
  g_deui_wifi.sta_retry_delay_ms = delay_ms;

  if (reopen_ap && !g_deui_wifi.info.ap_running) {
    if (deui_wifi_configure_ap(true) == ESP_OK) {
      (void)deui_wifi_start_portal();
      g_deui_wifi.portal_running = true;
      ESP_LOGW(TAG, "Re-opened setup AP after repeated STA disconnects");
    }
  }

  if (g_deui_wifi.reconnect_timer != NULL) {
    (void)esp_timer_stop(g_deui_wifi.reconnect_timer);
    (void)esp_timer_start_once(g_deui_wifi.reconnect_timer, (uint64_t)delay_ms * 1000ULL);
  } else {
    (void)deui_wifi_connect_station();
  }
}
