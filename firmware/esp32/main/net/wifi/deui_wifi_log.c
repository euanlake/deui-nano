#include "deui_wifi_log.h"

#include "deui_wifi_internal.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"

static const char *TAG = "deui_wifi_diag";

void deui_wifi_enable_verbose_logging(void) {
  esp_log_level_set("deui_wifi", ESP_LOG_DEBUG);
  esp_log_level_set("deui_wifi_ap", ESP_LOG_DEBUG);
  esp_log_level_set("deui_wifi_evt", ESP_LOG_DEBUG);
  esp_log_level_set("deui_wifi_diag", ESP_LOG_DEBUG);
  esp_log_level_set("deui_wifi_coex", ESP_LOG_DEBUG);
  esp_log_level_set("deui_wifi_http", ESP_LOG_INFO);
  esp_log_level_set("deui_ble", ESP_LOG_INFO);
  esp_log_level_set("wifi", ESP_LOG_DEBUG);
  esp_log_level_set("wifi_init", ESP_LOG_INFO);
  esp_log_level_set("net80211", ESP_LOG_DEBUG);
  esp_log_level_set("dhcps", ESP_LOG_DEBUG);
  esp_log_level_set("dhcpc", ESP_LOG_DEBUG);
  esp_log_level_set("esp_netif_lwip", ESP_LOG_INFO);
}

const char *deui_wifi_event_name(int32_t event_id) {
  switch (event_id) {
    case WIFI_EVENT_WIFI_READY:
      return "WIFI_READY";
    case WIFI_EVENT_SCAN_DONE:
      return "SCAN_DONE";
    case WIFI_EVENT_STA_START:
      return "STA_START";
    case WIFI_EVENT_STA_STOP:
      return "STA_STOP";
    case WIFI_EVENT_STA_CONNECTED:
      return "STA_CONNECTED";
    case WIFI_EVENT_STA_DISCONNECTED:
      return "STA_DISCONNECTED";
    case WIFI_EVENT_STA_AUTHMODE_CHANGE:
      return "STA_AUTHMODE_CHANGE";
    case WIFI_EVENT_AP_START:
      return "AP_START";
    case WIFI_EVENT_AP_STOP:
      return "AP_STOP";
    case WIFI_EVENT_AP_STACONNECTED:
      return "AP_STACONNECTED";
    case WIFI_EVENT_AP_STADISCONNECTED:
      return "AP_STADISCONNECTED";
    case WIFI_EVENT_AP_PROBEREQRECVED:
      return "AP_PROBEREQRECVED";
    case WIFI_EVENT_AP_WRONG_PASSWORD:
      return "AP_WRONG_PASSWORD";
    case WIFI_EVENT_HOME_CHANNEL_CHANGE:
      return "HOME_CHANNEL_CHANGE";
    case WIFI_EVENT_STA_BEACON_TIMEOUT:
      return "STA_BEACON_TIMEOUT";
    default:
      return "OTHER";
  }
}

void deui_wifi_log_join_state(const char *phase) {
  const int64_t now_us = esp_timer_get_time();
  int64_t boost_left_ms = 0;
  wifi_sta_list_t sta_list = {0};

  if (g_deui_wifi.wifi_join_boost_until_us > now_us) {
    boost_left_ms = (g_deui_wifi.wifi_join_boost_until_us - now_us) / 1000;
  }

  if (g_deui_wifi.info.ap_running) {
    (void)esp_wifi_ap_get_sta_list(&sta_list);
  }

  ESP_LOGI(TAG,
           "[%s] ssid=\"%s\" pass=\"%s\" ap_running=%d portal=%d ap_clients=%u driver_sta=%u "
           "await_dhcp=%d join_boost_ms=%lld sta_connected=%d",
           phase != NULL ? phase : "?", g_deui_wifi.info.ap_ssid, g_deui_wifi.info.ap_password,
           (int)g_deui_wifi.info.ap_running, (int)g_deui_wifi.portal_running,
           (unsigned)g_deui_wifi.ap_station_count, (unsigned)sta_list.num,
           (int)g_deui_wifi.ap_awaiting_dhcp, (long long)boost_left_ms,
           (int)g_deui_wifi.info.sta_connected);
}

const char *deui_wifi_authmode_name(wifi_auth_mode_t mode) {
  switch (mode) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA_WPA2_PSK";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3_PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2_WPA3_PSK";
    default:
      return "OTHER";
  }
}

const char *deui_wifi_reason_name(uint16_t reason) {
  switch (reason) {
    case WIFI_REASON_UNSPECIFIED:
      return "UNSPECIFIED";
    case WIFI_REASON_AUTH_EXPIRE:
      return "AUTH_EXPIRE";
    case WIFI_REASON_AUTH_LEAVE:
      return "AUTH_LEAVE";
    case WIFI_REASON_DISASSOC_DUE_TO_INACTIVITY:
      return "DISASSOC_INACTIVITY";
    case WIFI_REASON_ASSOC_TOOMANY:
      return "ASSOC_TOOMANY";
    case WIFI_REASON_CLASS2_FRAME_FROM_NONAUTH_STA:
      return "CLASS2_NONAUTH";
    case WIFI_REASON_CLASS3_FRAME_FROM_NONASSOC_STA:
      return "CLASS3_NONASSOC";
    case WIFI_REASON_ASSOC_LEAVE:
      return "ASSOC_LEAVE";
    case WIFI_REASON_ASSOC_NOT_AUTHED:
      return "ASSOC_NOT_AUTHED";
    case WIFI_REASON_IE_INVALID:
      return "IE_INVALID";
    case WIFI_REASON_MIC_FAILURE:
      return "MIC_FAILURE";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
      return "4WAY_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
      return "GROUP_KEY_TIMEOUT";
    case WIFI_REASON_IE_IN_4WAY_DIFFERS:
      return "IE_IN_4WAY_DIFFERS";
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
      return "PAIRWISE_CIPHER_INVALID";
    case WIFI_REASON_AKMP_INVALID:
      return "AKMP_INVALID";
    case WIFI_REASON_802_1X_AUTH_FAILED:
      return "802_1X_AUTH_FAILED";
    case WIFI_REASON_BEACON_TIMEOUT:
      return "BEACON_TIMEOUT";
    case WIFI_REASON_NO_AP_FOUND:
      return "NO_AP_FOUND";
    case WIFI_REASON_AUTH_FAIL:
      return "AUTH_FAIL";
    case WIFI_REASON_ASSOC_FAIL:
      return "ASSOC_FAIL";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      return "HANDSHAKE_TIMEOUT";
    case WIFI_REASON_CONNECTION_FAIL:
      return "CONNECTION_FAIL";
    default:
      return "UNKNOWN";
  }
}
