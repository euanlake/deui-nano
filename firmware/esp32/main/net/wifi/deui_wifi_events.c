#include "deui_wifi_internal.h"

#include "deui_wifi_log.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_wifi.h"

static const char *TAG = "deui_wifi_evt";

static int64_t s_last_probe_log_us;

static void log_probe_request(const wifi_event_ap_probe_req_rx_t *event) {
  int64_t now_us;
  if (event == NULL) {
    return;
  }

  deui_wifi_extend_join_boost(25 * 1000000LL, "probe");
  now_us = esp_timer_get_time();
  if (s_last_probe_log_us != 0 && (now_us - s_last_probe_log_us) < 500000) {
    ESP_LOGD(TAG, "Probe " MACSTR " rssi=%d", MAC2STR(event->mac), event->rssi);
    return;
  }
  s_last_probe_log_us = now_us;
  ESP_LOGI(TAG, ">>> JOIN ATTEMPT: probe from " MACSTR " rssi=%d dBm (phone scanning for AP)", MAC2STR(event->mac),
           event->rssi);
  deui_wifi_log_join_state("probe");
}

static void log_ap_sta_connected(const wifi_event_ap_staconnected_t *event) {
  if (event == NULL) {
    return;
  }
  ESP_LOGI(TAG,
           ">>> JOIN OK (L2): " MACSTR " aid=%u — WPA assoc complete, waiting for DHCP (pass=\"%s\")",
           MAC2STR(event->mac), (unsigned)event->aid, g_deui_wifi.info.ap_password);
}

static void log_ap_sta_disconnected(const wifi_event_ap_stadisconnected_t *event) {
  if (event == NULL) {
    return;
  }

  const bool auth_failure = event->reason == WIFI_REASON_AUTH_EXPIRE ||
                            event->reason == WIFI_REASON_AUTH_FAIL ||
                            event->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                            event->reason == WIFI_REASON_HANDSHAKE_TIMEOUT ||
                            event->reason == WIFI_REASON_MIC_FAILURE ||
                            event->reason == WIFI_REASON_PAIRWISE_CIPHER_INVALID ||
                            event->reason == WIFI_REASON_AKMP_INVALID ||
                            event->reason == WIFI_REASON_ASSOC_FAIL ||
                            event->reason == WIFI_REASON_CONNECTION_FAIL;

  if (auth_failure) {
    ESP_LOGE(TAG,
             ">>> JOIN FAILED: " MACSTR " aid=%u reason=%u (%s) — use SSID \"%s\" password \"%s\"",
             MAC2STR(event->mac), (unsigned)event->aid, (unsigned)event->reason,
             deui_wifi_reason_name(event->reason), g_deui_wifi.info.ap_ssid, g_deui_wifi.info.ap_password);
    deui_wifi_extend_join_boost(25 * 1000000LL, "auth_fail");
  } else {
    ESP_LOGW(TAG, ">>> CLIENT LEFT: " MACSTR " aid=%u reason=%u (%s)", MAC2STR(event->mac),
             (unsigned)event->aid, (unsigned)event->reason, deui_wifi_reason_name(event->reason));
  }
  deui_wifi_log_join_state(auth_failure ? "join_failed" : "client_left");
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data) {
  (void)arg;
  (void)base;

  switch (id) {
    case WIFI_EVENT_WIFI_READY:
      ESP_LOGI(TAG, "Wi-Fi driver ready");
      break;
    case WIFI_EVENT_SCAN_DONE: {
      wifi_event_sta_scan_done_t *event = (wifi_event_sta_scan_done_t *)event_data;
      if (event != NULL) {
        ESP_LOGI(TAG, "STA scan done status=%u num=%u", (unsigned)event->status, (unsigned)event->number);
      }
      break;
    }
    case WIFI_EVENT_STA_START:
      if (g_deui_wifi.info.has_saved_credentials) {
        ESP_LOGI(TAG, "STA interface started (will connect to saved network)");
        (void)deui_wifi_connect_station();
      } else {
        ESP_LOGI(TAG, "STA interface started (idle — setup uses SoftAP only, no home Wi-Fi yet)");
      }
      break;
    case WIFI_EVENT_STA_STOP:
      ESP_LOGI(TAG, "STA interface stopped");
      break;
    case WIFI_EVENT_STA_CONNECTED: {
      wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
      if (event != NULL) {
        ESP_LOGI(TAG, "STA connected to \"%s\" ch=%u auth=%s", event->ssid, (unsigned)event->channel,
                 deui_wifi_authmode_name(event->authmode));
      }
      break;
    }
    case WIFI_EVENT_STA_DISCONNECTED: {
      wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
      ESP_LOGW(TAG, "STA disconnected: reason=%u (%s) ssid=\"%s\"", event != NULL ? (unsigned)event->reason : 0U,
               event != NULL ? deui_wifi_reason_name(event->reason) : "n/a",
               event != NULL ? (const char *)event->ssid : "");
      if (event != NULL) {
        g_deui_wifi.sta_last_disconnect_reason = (uint8_t)event->reason;
      }
      deui_wifi_handle_sta_disconnect();
      break;
    }
    case WIFI_EVENT_STA_AUTHMODE_CHANGE: {
      wifi_event_sta_authmode_change_t *event = (wifi_event_sta_authmode_change_t *)event_data;
      if (event != NULL) {
        ESP_LOGI(TAG, "STA authmode change: %s -> %s", deui_wifi_authmode_name(event->old_mode),
                 deui_wifi_authmode_name(event->new_mode));
      }
      break;
    }
    case WIFI_EVENT_AP_START:
      g_deui_wifi.info.ap_running = true;
      g_deui_wifi.ap_station_count = 0;
      g_deui_wifi.ap_awaiting_dhcp = false;
      g_deui_wifi.ap_dhcp_done = false;
      g_deui_wifi.wifi_join_boost_until_us = 0;
      ESP_LOGI(TAG, "SoftAP started — phones should join \"%s\" (WPA2, pass=\"%s\")", g_deui_wifi.info.ap_ssid,
               g_deui_wifi.info.ap_password);
      deui_wifi_log_running_ap_config();
      deui_wifi_log_join_state("ap_start");
      deui_wifi_apply_coex_preference();
      break;
    case WIFI_EVENT_AP_STOP:
      g_deui_wifi.info.ap_running = false;
      g_deui_wifi.ap_station_count = 0;
      g_deui_wifi.ap_awaiting_dhcp = false;
      g_deui_wifi.ap_dhcp_done = false;
      g_deui_wifi.wifi_join_boost_until_us = 0;
      ESP_LOGI(TAG, "SoftAP stopped");
      deui_wifi_log_join_state("ap_stop");
      deui_wifi_apply_coex_preference();
      break;
    case WIFI_EVENT_AP_STACONNECTED: {
      wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
      if (event != NULL) {
        if (g_deui_wifi.ap_station_count < 255) {
          ++g_deui_wifi.ap_station_count;
        }
        log_ap_sta_connected(event);
        deui_wifi_note_ap_client_associated("sta_connected");
      }
      break;
    }
    case WIFI_EVENT_AP_STADISCONNECTED: {
      wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
      if (event != NULL) {
        if (g_deui_wifi.ap_station_count > 0) {
          --g_deui_wifi.ap_station_count;
        }
        log_ap_sta_disconnected(event);
        if (g_deui_wifi.ap_station_count == 0) {
          deui_wifi_note_ap_client_gone("sta_disconnected");
        }
      }
      deui_wifi_apply_coex_preference();
      break;
    }
    case WIFI_EVENT_AP_PROBEREQRECVED:
      log_probe_request((const wifi_event_ap_probe_req_rx_t *)event_data);
      break;
    case WIFI_EVENT_AP_WRONG_PASSWORD: {
      wifi_event_ap_wrong_password_t *event = (wifi_event_ap_wrong_password_t *)event_data;
      if (event != NULL) {
        ESP_LOGE(TAG, ">>> WRONG PASSWORD from " MACSTR " (expected \"%s\")", MAC2STR(event->mac),
                 g_deui_wifi.info.ap_password);
      } else {
        ESP_LOGE(TAG, ">>> WRONG PASSWORD (no MAC in event)");
      }
      deui_wifi_extend_join_boost(25 * 1000000LL, "wrong_password");
      break;
    }
    case WIFI_EVENT_HOME_CHANNEL_CHANGE:
      ESP_LOGI(TAG, "Home channel change");
      break;
    case WIFI_EVENT_STA_BEACON_TIMEOUT:
      ESP_LOGW(TAG, "STA beacon timeout");
      break;
    default:
      ESP_LOGI(TAG, "Wi-Fi event %s (id=%ld)", deui_wifi_event_name(id), (long)id);
      break;
  }
}

static void ip_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data) {
  (void)arg;
  (void)base;

  if (id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (event != NULL) {
      ESP_LOGI(TAG, "STA got IP: " IPSTR " gateway=" IPSTR, IP2STR(&event->ip_info.ip),
               IP2STR(&event->ip_info.gw));
    }
    deui_wifi_note_got_ip(event);
    deui_wifi_mdns_on_got_ip();
    g_deui_wifi.wifi_join_boost_until_us = 0;
    deui_wifi_apply_coex_preference();
    if (g_deui_wifi.info.ap_running) {
      deui_wifi_provision_on_got_ip();
      ESP_LOGI(TAG, "STA online — setup AP will stop shortly (portal stays up for status page)");
    }
  } else if (id == IP_EVENT_AP_STAIPASSIGNED) {
    ip_event_ap_staipassigned_t *event = (ip_event_ap_staipassigned_t *)event_data;
    if (event != NULL) {
      ESP_LOGI(TAG, ">>> JOIN COMPLETE: DHCP gave client " IPSTR " — open " DEUI_WIFI_PORTAL_URL,
               IP2STR(&event->ip));
    } else {
      ESP_LOGI(TAG, ">>> JOIN COMPLETE: DHCP assigned (no IP in event)");
    }
    g_deui_wifi.ap_awaiting_dhcp = false;
    g_deui_wifi.ap_dhcp_done = true;
    deui_wifi_log_join_state("dhcp_ok");
    deui_wifi_extend_join_boost(8 * 1000000LL, "dhcp_ok");
  } else {
    ESP_LOGI(TAG, "IP event id=%ld", (long)id);
  }
}

esp_err_t deui_wifi_register_events(void) {
  ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL), TAG,
                      "Register Wi-Fi handler failed");
  ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL), TAG,
                      "Register got-IP handler failed");
  ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &ip_event_handler, NULL), TAG,
                      "Register AP-IP handler failed");
  ESP_LOGI(TAG, "Wi-Fi/IP event handlers registered (join logging enabled)");
  return ESP_OK;
}
