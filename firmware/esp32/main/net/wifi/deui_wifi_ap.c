#include "deui_wifi_internal.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "deui_wifi_log.h"

static const char *TAG = "deui_wifi_ap";

void deui_wifi_fill_ap_ssid(char *out, size_t out_size) {
  uint8_t mac[6] = {0};

  if (out == NULL || out_size == 0) {
    return;
  }
  if (esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP) == ESP_OK) {
    (void)snprintf(out, out_size, "DEUI-%02X%02X", mac[4], mac[5]);
    return;
  }
  strlcpy(out, "DEUI-SETUP", out_size);
}

static void log_ap_config(const wifi_config_t *cfg) {
  if (cfg == NULL) {
    return;
  }
  ESP_LOGI(TAG,
           "AP config: ssid=\"%s\" len=%u auth=%s(%d) channel=%u max_conn=%u pmf_required=%d",
           cfg->ap.ssid, (unsigned)cfg->ap.ssid_len, deui_wifi_authmode_name(cfg->ap.authmode),
           (int)cfg->ap.authmode, (unsigned)cfg->ap.channel, (unsigned)cfg->ap.max_connection,
           (int)cfg->ap.pmf_cfg.required);
}

esp_err_t deui_wifi_configure_ap(bool apsta_mode) {
  wifi_config_t ap_cfg = {0};
  wifi_mode_t mode = apsta_mode ? WIFI_MODE_APSTA : WIFI_MODE_AP;
  const size_t pass_len = strlen(g_deui_wifi.info.ap_password);

  strlcpy((char *)ap_cfg.ap.ssid, g_deui_wifi.info.ap_ssid, sizeof(ap_cfg.ap.ssid));
  strlcpy((char *)ap_cfg.ap.password, g_deui_wifi.info.ap_password, sizeof(ap_cfg.ap.password));
  ap_cfg.ap.ssid_len = strlen(g_deui_wifi.info.ap_ssid);
  ap_cfg.ap.channel = 1;
  ap_cfg.ap.max_connection = 4;
  ap_cfg.ap.ssid_hidden = 0;
  /* PMF optional only — required PMF breaks association on many phones with ESP32 SoftAP. */
  ap_cfg.ap.pmf_cfg.capable = true;
  ap_cfg.ap.pmf_cfg.required = false;

  if (pass_len < 8) {
    ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    ap_cfg.ap.password[0] = '\0';
    ap_cfg.ap.pmf_cfg.capable = false;
    ap_cfg.ap.pmf_cfg.required = false;
    ESP_LOGW(TAG, "AP password shorter than 8 chars; using OPEN auth for setup");
  } else {
    ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
  }

  ESP_RETURN_ON_ERROR(esp_wifi_set_mode(mode), TAG, "Set AP mode failed");
  ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg), TAG, "Set AP config failed");

  log_ap_config(&ap_cfg);
  {
    uint8_t bssid[6] = {0};
    if (esp_wifi_get_mac(WIFI_IF_AP, bssid) == ESP_OK) {
      ESP_LOGI(TAG, "AP BSSID " MACSTR " — join SSID \"%s\" with password \"%s\"", MAC2STR(bssid),
               g_deui_wifi.info.ap_ssid, g_deui_wifi.info.ap_password);
    }
  }
  g_deui_wifi.info.ap_running = true;
  return ESP_OK;
}

esp_err_t deui_wifi_disable_ap(void) {
  ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Set STA mode failed");
  g_deui_wifi.info.ap_running = false;
  ESP_LOGI(TAG, "Setup AP stopped (STA-only mode)");
  return ESP_OK;
}

void deui_wifi_log_running_ap_config(void) {
  wifi_config_t cfg = {0};
  if (esp_wifi_get_config(WIFI_IF_AP, &cfg) == ESP_OK) {
    log_ap_config(&cfg);
  }
}
