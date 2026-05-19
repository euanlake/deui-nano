#include "deui_wifi_internal.h"

#include "esp_check.h"

esp_err_t deui_wifi_start_portal(void) {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();

  if (g_deui_wifi.httpd != NULL) {
    return ESP_OK;
  }

  cfg.uri_match_fn = httpd_uri_match_wildcard;
  /* HTTP routes + fonts + images + favicon; keep headroom. */
  cfg.max_uri_handlers = 26;
  /* LWIP_MAX_SOCKETS caps httpd at 7; do not raise max_open_sockets above default. */
  /* Portal HTML + httpd framing; default 4096 overflows on first page load. */
  cfg.stack_size = 10240;
  ESP_RETURN_ON_ERROR(httpd_start(&g_deui_wifi.httpd, &cfg), "deui_wifi_portal", "HTTP start failed");
  ESP_RETURN_ON_ERROR(deui_wifi_portal_register_handlers(g_deui_wifi.httpd), "deui_wifi_portal",
                      "HTTP route registration failed");
  g_deui_wifi.portal_running = true;
  return ESP_OK;
}

esp_err_t deui_wifi_stop_portal(void) {
  esp_err_t err;

  if (g_deui_wifi.httpd == NULL) {
    g_deui_wifi.portal_running = false;
    return ESP_OK;
  }
  err = httpd_stop(g_deui_wifi.httpd);
  if (err == ESP_OK) {
    g_deui_wifi.httpd = NULL;
    g_deui_wifi.portal_running = false;
  }
  return err;
}
