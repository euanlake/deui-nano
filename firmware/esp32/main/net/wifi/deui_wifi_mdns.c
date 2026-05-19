#include "deui_wifi_internal.h"

#include <strings.h>
#include <string.h>

#include "esp_log.h"
#include "mdns.h"

static const char *TAG = "deui_mdns";

static bool s_mdns_started;
static bool s_http_service_added;

static void normalize_hostname(char *host, size_t host_size) {
  size_t len;

  if (host == NULL || host_size == 0) {
    return;
  }
  len = strlen(host);
  if (len > 6 && strcasecmp(host + len - 6, ".local") == 0) {
    host[len - 6] = '\0';
  }
  if (host[0] == '\0') {
    strlcpy(host, DEUI_WIFI_DEFAULT_HOSTNAME, host_size);
  }
}

esp_err_t deui_wifi_mdns_init(void) {
  esp_err_t err;

  if (s_mdns_started) {
    return deui_wifi_mdns_apply_hostname();
  }

  err = mdns_init();
  if (err == ESP_ERR_INVALID_STATE) {
    /* Already initialized (e.g. warm path); treat as success. */
    s_mdns_started = true;
    return deui_wifi_mdns_apply_hostname();
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(err));
    return err;
  }
  s_mdns_started = true;
  return deui_wifi_mdns_apply_hostname();
}

esp_err_t deui_wifi_mdns_apply_hostname(void) {
  char host[sizeof(g_deui_wifi.info.hostname)];

  if (!s_mdns_started) {
    return ESP_ERR_INVALID_STATE;
  }

  strlcpy(host, g_deui_wifi.info.hostname, sizeof(host));
  normalize_hostname(host, sizeof(host));

  esp_err_t err = mdns_hostname_set(host);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "mDNS hostname set failed: %s", esp_err_to_name(err));
    return err;
  }
  (void)mdns_instance_name_set("DEUI Controller");
  if (!s_http_service_added) {
    esp_err_t svc_err = mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    if (svc_err == ESP_OK) {
      s_http_service_added = true;
    } else {
      ESP_LOGW(TAG, "mDNS HTTP service add failed: %s", esp_err_to_name(svc_err));
    }
  }
  ESP_LOGI(TAG, "mDNS: reachable as %s.local on the local network", host);
  return ESP_OK;
}

void deui_wifi_mdns_on_got_ip(void) {
  if (!s_mdns_started) {
    return;
  }
  (void)deui_wifi_mdns_apply_hostname();
}
