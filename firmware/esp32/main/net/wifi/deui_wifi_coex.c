#include "deui_wifi_internal.h"

#include <string.h>

#include "deui_ble_client.h"
#include "deui_wifi_log.h"
#include "esp_coexist.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"

static const char *TAG = "deui_wifi_coex";

/** Hold radio for DHCP after L2 assoc (phones drop with reason 15 if BLE scans too soon). */
static const int64_t k_dhcp_hold_us = 60 * 1000000LL;

#if CONFIG_ESP_COEX_SW_COEXIST_ENABLE
static esp_coex_prefer_t s_last_coex_pref = (esp_coex_prefer_t)-1;
#endif

bool deui_wifi_ap_has_station(void) {
  return g_deui_wifi.ap_station_count > 0;
}

bool deui_wifi_ap_awaiting_dhcp(void) {
  return g_deui_wifi.ap_awaiting_dhcp;
}

bool deui_wifi_join_boost_active(void) {
  return esp_timer_get_time() < g_deui_wifi.wifi_join_boost_until_us;
}

bool deui_wifi_block_ble_scan(void) {
  return deui_wifi_join_boost_active() || g_deui_wifi.ap_awaiting_dhcp;
}

void deui_wifi_note_ap_client_associated(const char *reason) {
  if (!g_deui_wifi.info.ap_running) {
    return;
  }
  if (g_deui_wifi.ap_awaiting_dhcp || g_deui_wifi.ap_dhcp_done) {
    return;
  }
  g_deui_wifi.ap_awaiting_dhcp = true;
  ESP_LOGI(TAG, "AP client associated (%s) — blocking BLE until DHCP", reason != NULL ? reason : "?");
  deui_wifi_extend_join_boost(k_dhcp_hold_us, reason != NULL ? reason : "assoc");
  deui_wifi_log_join_state("assoc");
}

void deui_wifi_note_ap_client_gone(const char *reason) {
  if (!g_deui_wifi.ap_awaiting_dhcp && g_deui_wifi.ap_station_count == 0) {
    return;
  }
  g_deui_wifi.ap_awaiting_dhcp = false;
  g_deui_wifi.ap_dhcp_done = false;
  ESP_LOGI(TAG, "AP client gone (%s) — BLE may resume", reason != NULL ? reason : "?");
  deui_wifi_log_join_state("gone");
  deui_wifi_apply_coex_preference();
}

void deui_wifi_poll_ap_clients(void) {
  wifi_sta_list_t sta_list = {0};

  if (!g_deui_wifi.info.ap_running || !g_deui_wifi.portal_running) {
    return;
  }
  if (esp_wifi_ap_get_sta_list(&sta_list) != ESP_OK) {
    return;
  }

  if (sta_list.num > 0 && !g_deui_wifi.ap_awaiting_dhcp && !g_deui_wifi.ap_dhcp_done) {
    if (g_deui_wifi.ap_station_count < sta_list.num) {
      g_deui_wifi.ap_station_count = (uint8_t)sta_list.num;
    }
    deui_wifi_note_ap_client_associated("driver_poll");
    return;
  }

  if (sta_list.num == 0 && g_deui_wifi.ap_awaiting_dhcp) {
    g_deui_wifi.ap_station_count = 0;
    deui_wifi_note_ap_client_gone("driver_poll");
  }
}

void deui_wifi_extend_join_boost(int64_t duration_us, const char *reason) {
  const int64_t until = esp_timer_get_time() + duration_us;
  const int64_t added_ms = duration_us / 1000;

  if (until > g_deui_wifi.wifi_join_boost_until_us) {
    g_deui_wifi.wifi_join_boost_until_us = until;
  }

  ESP_LOGI(TAG, "Join boost +%lld ms (%s)", (long long)added_ms, reason != NULL ? reason : "?");
  if (reason != NULL && strcmp(reason, "scan_quiet") == 0) {
    ESP_LOGI(TAG, ">>> JOIN WINDOW: connect phone to \"%s\" now (pass \"%s\") — BLE paused ~%lld s",
             g_deui_wifi.info.ap_ssid, g_deui_wifi.info.ap_password, (long long)(added_ms / 1000));
  }
  deui_wifi_log_join_state(reason != NULL ? reason : "boost");
  deui_ble_yield_radio_for_wifi();
  deui_wifi_apply_coex_preference();
}

void deui_wifi_apply_coex_preference(void) {
#if CONFIG_ESP_COEX_SW_COEXIST_ENABLE
  const bool prefer_wifi = deui_wifi_block_ble_scan();
  const esp_coex_prefer_t pref = prefer_wifi ? ESP_COEX_PREFER_WIFI : ESP_COEX_PREFER_BT;

  if (pref != s_last_coex_pref) {
    esp_coex_preference_set(pref);
    s_last_coex_pref = pref;
    ESP_LOGI(TAG, "Coexist preference -> %s", prefer_wifi ? "Wi-Fi" : "BLE");
    deui_wifi_log_join_state(prefer_wifi ? "coex_wifi" : "coex_ble");
  }
#else
  (void)TAG;
#endif
}
