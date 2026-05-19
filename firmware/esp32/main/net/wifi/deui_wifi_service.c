#include "deui_wifi_internal.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_coexist.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "deui_wifi_log.h"

static const char *TAG = "deui_wifi";

deui_wifi_state_t g_deui_wifi = {
  .info.ap_running = false,
  .info.sta_connected = false,
  .info.sta_connecting = false,
  .info.has_saved_credentials = false,
  .info.ap_ssid = "DEUI-SETUP",
  .info.ap_password = DEUI_WIFI_PORTAL_PASS,
  .info.sta_ip = "",
  .info.hostname = DEUI_WIFI_DEFAULT_HOSTNAME,
  .dns_socket = -1,
};

static esp_err_t init_nvs(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "NVS erase failed");
    err = nvs_flash_init();
  }
  return err;
}

static void reconnect_timer_cb(void *arg) {
  (void)arg;
  (void)deui_wifi_connect_station();
}

static esp_err_t ensure_reconnect_timer(void) {
  if (g_deui_wifi.reconnect_timer != NULL) {
    return ESP_OK;
  }
  esp_timer_create_args_t args = {
    .callback = reconnect_timer_cb,
    .name = "deui_wifi_reconnect",
  };
  return esp_timer_create(&args, &g_deui_wifi.reconnect_timer);
}

esp_err_t deui_wifi_ensure_sta_netif(void) {
  if (g_deui_wifi.sta_netif != NULL) {
    return ESP_OK;
  }
  g_deui_wifi.sta_netif = esp_netif_create_default_wifi_sta();
  ESP_RETURN_ON_FALSE(g_deui_wifi.sta_netif != NULL, ESP_ERR_NO_MEM, TAG, "STA netif create failed");
  return ESP_OK;
}

esp_err_t deui_wifi_init(void) {
  wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t err;
  bool has_saved_credentials = false;

  if (g_deui_wifi.initialized) {
    return ESP_OK;
  }

  deui_wifi_enable_verbose_logging();

  ESP_RETURN_ON_ERROR(init_nvs(), TAG, "NVS init failed");
  ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "Netif init failed");
  err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }

  g_deui_wifi.ap_netif = esp_netif_create_default_wifi_ap();
  ESP_RETURN_ON_FALSE(g_deui_wifi.ap_netif != NULL, ESP_ERR_NO_MEM, TAG, "AP netif create failed");

  ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "Wi-Fi init failed");
  ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "Set storage mode failed");
  ESP_RETURN_ON_ERROR(deui_wifi_register_events(), TAG, "Register events failed");
  ESP_RETURN_ON_ERROR(ensure_reconnect_timer(), TAG, "Reconnect timer init failed");

  deui_wifi_fill_ap_ssid(g_deui_wifi.info.ap_ssid, sizeof(g_deui_wifi.info.ap_ssid));
  strlcpy(g_deui_wifi.info.ap_password, DEUI_WIFI_PORTAL_PASS, sizeof(g_deui_wifi.info.ap_password));

  g_deui_wifi.sta_ssid[0] = '\0';
  g_deui_wifi.sta_password[0] = '\0';
  strlcpy(g_deui_wifi.info.hostname, DEUI_WIFI_DEFAULT_HOSTNAME, sizeof(g_deui_wifi.info.hostname));
  err = deui_wifi_settings_load(g_deui_wifi.sta_ssid, sizeof(g_deui_wifi.sta_ssid), g_deui_wifi.sta_password,
                                sizeof(g_deui_wifi.sta_password), g_deui_wifi.info.hostname,
                                sizeof(g_deui_wifi.info.hostname));
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Wi-Fi settings load failed: %s", esp_err_to_name(err));
  }
  has_saved_credentials = g_deui_wifi.sta_ssid[0] != '\0';
  g_deui_wifi.info.has_saved_credentials = has_saved_credentials;

  ESP_RETURN_ON_ERROR(deui_wifi_ensure_sta_netif(), TAG, "STA netif create failed");
  if (!has_saved_credentials) {
    /* APSTA during setup (La Marzocco-style) — STA idle, SoftAP for portal. */
    ESP_RETURN_ON_ERROR(deui_wifi_configure_ap(true), TAG, "Configure setup AP failed");
  } else {
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Set STA mode failed");
  }

  ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start failed");
  ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE), TAG, "Disable Wi-Fi power save failed");
  deui_wifi_apply_coex_preference();

  err = deui_wifi_mdns_init();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "mDNS init failed (%s); Wi-Fi will run without .local hostname", esp_err_to_name(err));
  }

  if (!has_saved_credentials) {
    ESP_RETURN_ON_ERROR(deui_wifi_start_portal(), TAG, "Portal start failed");
    g_deui_wifi.portal_running = true;
    g_deui_wifi.info.ap_running = true;
    g_deui_wifi.info.sta_connecting = false;
    ESP_LOGI(TAG, "DEUI setup AP started: SSID=%s pass=%s portal=%s", g_deui_wifi.info.ap_ssid,
             g_deui_wifi.info.ap_password, DEUI_WIFI_PORTAL_URL);
    ESP_LOGI(TAG, "Join with WPA2 password \"%s\", then open %s (no captive popup)", g_deui_wifi.info.ap_password,
             DEUI_WIFI_PORTAL_URL);
    deui_wifi_log_join_state("portal_up");
    deui_wifi_log_running_ap_config();
  } else {
    ESP_RETURN_ON_ERROR(deui_wifi_apply_station_config(false), TAG, "Apply STA config failed");
    ESP_RETURN_ON_ERROR(deui_wifi_connect_station(), TAG, "Initial STA connect failed");
    g_deui_wifi.info.sta_connecting = true;
    g_deui_wifi.info.ap_running = false;
  }

  g_deui_wifi.initialized = true;
  g_deui_wifi.suspended = false;
  return ESP_OK;
}

esp_err_t deui_wifi_suspend(void) {
  esp_err_t err;

  if (!g_deui_wifi.initialized || g_deui_wifi.suspended) {
    return ESP_OK;
  }
  (void)deui_wifi_stop_portal();
  deui_wifi_stop_captive_dns();
  if (g_deui_wifi.reconnect_timer != NULL) {
    (void)esp_timer_stop(g_deui_wifi.reconnect_timer);
  }
  err = esp_wifi_stop();
  if (err != ESP_OK) {
    return err;
  }

  g_deui_wifi.info.sta_connected = false;
  g_deui_wifi.info.sta_connecting = false;
  g_deui_wifi.info.ap_running = false;
  g_deui_wifi.info.sta_ip[0] = '\0';
  g_deui_wifi.suspended = true;
  ESP_LOGI(TAG, "Wi-Fi suspended");
  return ESP_OK;
}

esp_err_t deui_wifi_resume(void) {
  if (!g_deui_wifi.initialized || !g_deui_wifi.suspended) {
    return ESP_OK;
  }

  ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start failed on resume");
  if (!g_deui_wifi.info.has_saved_credentials) {
    ESP_RETURN_ON_ERROR(deui_wifi_start_portal(), TAG, "Portal start failed on resume");
    g_deui_wifi.portal_running = true;
    g_deui_wifi.info.ap_running = true;
  } else {
    ESP_RETURN_ON_ERROR(deui_wifi_apply_station_config(false), TAG, "STA config failed on resume");
    (void)deui_wifi_connect_station();
    g_deui_wifi.info.sta_connecting = true;
  }
  g_deui_wifi.suspended = false;
  ESP_LOGI(TAG, "Wi-Fi resumed");
  return ESP_OK;
}

bool deui_wifi_is_suspended(void) {
  return g_deui_wifi.suspended;
}

bool deui_wifi_is_provisioning(void) {
  return g_deui_wifi.portal_running && !g_deui_wifi.info.has_saved_credentials;
}

void deui_wifi_get_info(deui_wifi_info_t *info) {
  if (info == NULL) {
    return;
  }
  *info = g_deui_wifi.info;
}
