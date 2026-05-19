#include "deui_wifi_internal.h"

#include "deui_wifi_log.h"

#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "deui_wifi_prov";

#define DEUI_WIFI_PROVISION_TIMEOUT_US (45LL * 1000000LL)
#define DEUI_WIFI_PROVISION_TEARDOWN_DELAY_US (8LL * 1000000LL)
#define DEUI_WIFI_PROVISION_FAIL_DISCONNECTS 4U

static void setup_teardown_timer_cb(void *arg) {
  (void)arg;

  if (!g_deui_wifi.info.sta_connected) {
    return;
  }
  (void)deui_wifi_stop_portal();
  deui_wifi_stop_captive_dns();
  (void)deui_wifi_disable_ap();
  g_deui_wifi.portal_running = false;
  g_deui_wifi.sta_provision_active = false;
  ESP_LOGI(TAG, "Setup AP and portal stopped after successful join");
}

static esp_err_t ensure_setup_teardown_timer(void) {
  if (g_deui_wifi.setup_teardown_timer != NULL) {
    return ESP_OK;
  }
  const esp_timer_create_args_t args = {
      .callback = setup_teardown_timer_cb,
      .name = "deui_wifi_ap_down",
  };
  return esp_timer_create(&args, &g_deui_wifi.setup_teardown_timer);
}

void deui_wifi_provision_begin(void) {
  g_deui_wifi.sta_provision_active = true;
  g_deui_wifi.sta_provision_failed = false;
  g_deui_wifi.sta_provision_started_us = esp_timer_get_time();
  g_deui_wifi.sta_last_disconnect_reason = 0;
  g_deui_wifi.sta_disconnect_count = 0;
  ESP_LOGI(TAG, "STA provision started for \"%s\"", g_deui_wifi.sta_ssid);
}

void deui_wifi_provision_on_got_ip(void) {
  if (!g_deui_wifi.sta_provision_active) {
    return;
  }
  g_deui_wifi.sta_provision_failed = false;
  if (ensure_setup_teardown_timer() == ESP_OK && g_deui_wifi.setup_teardown_timer != NULL) {
    (void)esp_timer_stop(g_deui_wifi.setup_teardown_timer);
    (void)esp_timer_start_once(g_deui_wifi.setup_teardown_timer, DEUI_WIFI_PROVISION_TEARDOWN_DELAY_US);
    ESP_LOGI(TAG, "Scheduled setup AP teardown in %lld s", (long long)(DEUI_WIFI_PROVISION_TEARDOWN_DELAY_US / 1000000LL));
  }
}

void deui_wifi_provision_note_sta_disconnect(uint8_t reason) {
  g_deui_wifi.sta_last_disconnect_reason = reason;

  if (!g_deui_wifi.sta_provision_active || g_deui_wifi.sta_provision_failed) {
    return;
  }
  if (g_deui_wifi.sta_disconnect_count >= DEUI_WIFI_PROVISION_FAIL_DISCONNECTS) {
    g_deui_wifi.sta_provision_failed = true;
    g_deui_wifi.sta_provision_active = false;
    ESP_LOGW(TAG, "STA provision failed after %u disconnect(s), last reason=%u (%s)",
             (unsigned)g_deui_wifi.sta_disconnect_count, (unsigned)reason, deui_wifi_reason_name(reason));
  }
}

static const char *provision_phase_name(void) {
  const int64_t now_us = esp_timer_get_time();

  if (g_deui_wifi.info.sta_connected && g_deui_wifi.info.sta_ip[0] != '\0') {
    return "success";
  }
  if (g_deui_wifi.sta_provision_failed) {
    return "failed";
  }
  if (g_deui_wifi.sta_provision_active) {
    if (g_deui_wifi.sta_provision_started_us > 0 &&
        (now_us - g_deui_wifi.sta_provision_started_us) > DEUI_WIFI_PROVISION_TIMEOUT_US &&
        !g_deui_wifi.info.sta_connected) {
      g_deui_wifi.sta_provision_failed = true;
      g_deui_wifi.sta_provision_active = false;
      return "failed";
    }
    return "connecting";
  }
  return "idle";
}

static void json_escape(const char *src, char *dst, size_t dst_size) {
  size_t out = 0;

  if (src == NULL || dst == NULL || dst_size == 0) {
    return;
  }
  while (*src != '\0' && out + 2 < dst_size) {
    if (*src == '"' || *src == '\\') {
      if (out + 2 >= dst_size) {
        break;
      }
      dst[out++] = '\\';
    }
    dst[out++] = *src++;
  }
  dst[out] = '\0';
}

void deui_wifi_provision_write_status_json(char *buf, size_t buf_size) {
  const char *phase = provision_phase_name();
  char ssid_esc[68];
  char msg_raw[128];
  char msg_esc[160];
  const char *message = "";

  if (buf == NULL || buf_size == 0) {
    return;
  }

  json_escape(g_deui_wifi.sta_ssid, ssid_esc, sizeof(ssid_esc));

  if (strcmp(phase, "success") == 0) {
    message = "DEUI joined your Wi-Fi network.";
  } else if (strcmp(phase, "failed") == 0) {
    if (g_deui_wifi.sta_last_disconnect_reason != 0) {
      (void)snprintf(msg_raw, sizeof(msg_raw), "Could not join. %s",
                     deui_wifi_reason_name(g_deui_wifi.sta_last_disconnect_reason));
      message = msg_raw;
    } else if (g_deui_wifi.sta_provision_started_us > 0 &&
               (esp_timer_get_time() - g_deui_wifi.sta_provision_started_us) > DEUI_WIFI_PROVISION_TIMEOUT_US) {
      message = "Timed out joining the network. Check the password and signal, then try again.";
    } else {
      message = "Could not join the network. Check the password and try again.";
    }
  } else if (strcmp(phase, "connecting") == 0) {
    message = "Joining your Wi-Fi network...";
  } else {
    message = "";
  }

  json_escape(message, msg_esc, sizeof(msg_esc));

  (void)snprintf(buf, buf_size,
                 "{\"phase\":\"%s\",\"ssid\":\"%s\",\"ip\":\"%s\",\"reason\":%u,\"message\":\"%s\"}", phase,
                 ssid_esc, g_deui_wifi.info.sta_ip, (unsigned)g_deui_wifi.sta_last_disconnect_reason, msg_esc);
}
