#include "deui_wifi_internal.h"
#include "deui_wifi_portal_assets.h"
#include "deui_wifi_portal_fonts.h"
#include "deui_wifi_portal_html.h"
#include "deui_wifi.h"
#include "deui_ota.h"
#include "deui_weight_stop.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "deui_wifi_http";

static esp_err_t redirect_to(httpd_req_t *req, const char *location) {
  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_hdr(req, "Location", location);
  return httpd_resp_send(req, NULL, 0);
}

static int from_hex(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  ch = (char)toupper((int)ch);
  if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  return -1;
}

static void url_decode(const char *src, char *dst, size_t dst_size) {
  size_t out = 0;
  while (src != NULL && *src != '\0' && out + 1 < dst_size) {
    if (*src == '+' && out + 1 < dst_size) {
      dst[out++] = ' ';
      src++;
      continue;
    }
    if (*src == '%' && src[1] != '\0' && src[2] != '\0') {
      int hi = from_hex(src[1]);
      int lo = from_hex(src[2]);
      if (hi >= 0 && lo >= 0) {
        dst[out++] = (char)((hi << 4) | lo);
        src += 3;
        continue;
      }
    }
    dst[out++] = *src++;
  }
  dst[out] = '\0';
}

static bool parse_form_field(const char *body, const char *key, char *dst, size_t dst_size) {
  char needle[32];
  const char *start;
  const char *end;
  size_t encoded_len;
  char encoded[96];

  if (body == NULL || key == NULL || dst == NULL || dst_size == 0) {
    return false;
  }
  (void)snprintf(needle, sizeof(needle), "%s=", key);
  start = strstr(body, needle);
  if (start == NULL) {
    dst[0] = '\0';
    return false;
  }
  start += strlen(needle);
  end = strchr(start, '&');
  if (end == NULL) {
    end = start + strlen(start);
  }
  encoded_len = (size_t)(end - start);
  if (encoded_len >= sizeof(encoded)) {
    encoded_len = sizeof(encoded) - 1;
  }
  memcpy(encoded, start, encoded_len);
  encoded[encoded_len] = '\0';
  url_decode(encoded, dst, dst_size);
  return true;
}

static esp_err_t handle_stop_weight_get(httpd_req_t *req) {
  char resp[64];
  const float grams = deui_weight_stop_get_target_g();

  (void)snprintf(resp, sizeof(resp), "{\"grams\":%.0f,\"enabled\":%s}", grams <= 0.0f ? 0.0f : grams,
                 grams > 0.0f ? "true" : "false");
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_stop_weight_post(httpd_req_t *req) {
  char body[64] = {0};
  char delta_str[16] = {0};
  char grams_str[16] = {0};
  int delta = 0;
  float grams = 0.0f;
  float new_g = 0.0f;
  char resp[64];

  int read = httpd_req_recv(req, body, sizeof(body) - 1);
  if (read <= 0) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty payload");
  }

  char enabled_str[8] = {0};
  if (parse_form_field(body, "enabled", enabled_str, sizeof(enabled_str))) {
    const bool enable = (enabled_str[0] == '1' || enabled_str[0] == 't' || enabled_str[0] == 'T');
    if (deui_weight_stop_set_enabled(enable) != ESP_OK) {
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Save failed");
    }
    new_g = deui_weight_stop_get_target_g();
  } else if (parse_form_field(body, "delta", delta_str, sizeof(delta_str))) {
    delta = atoi(delta_str);
    new_g = deui_weight_stop_adjust_target_g(delta);
  } else if (parse_form_field(body, "grams", grams_str, sizeof(grams_str))) {
    grams = (float)atof(grams_str);
    if (deui_weight_stop_set_target_g(grams) != ESP_OK) {
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Save failed");
    }
    new_g = deui_weight_stop_get_target_g();
  } else {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing enabled, delta, or grams");
  }

  (void)snprintf(resp, sizeof(resp), "{\"grams\":%.0f,\"enabled\":%s}", new_g <= 0.0f ? 0.0f : new_g,
                 new_g > 0.0f ? "true" : "false");
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

static void parse_prefill_ssid(httpd_req_t *req, char *prefill_ssid, size_t prefill_size) {
  char query[96] = {0};
  char encoded[66] = {0};

  if (prefill_ssid == NULL || prefill_size == 0) {
    return;
  }
  prefill_ssid[0] = '\0';
  if (httpd_req_get_url_query_len(req) > 0 &&
      httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
      httpd_query_key_value(query, "ssid", encoded, sizeof(encoded)) == ESP_OK) {
    url_decode(encoded, prefill_ssid, prefill_size);
  }
}

extern const uint8_t deui_logo_png_start[] asm("_binary_deui_logo_png_start");
extern const uint8_t deui_logo_png_end[] asm("_binary_deui_logo_png_end");

static esp_err_t handle_favicon_get(httpd_req_t *req) {
  const size_t len = (size_t)(deui_logo_png_end - deui_logo_png_start);
  httpd_resp_set_type(req, "image/png");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
  return httpd_resp_send(req, (const char *)deui_logo_png_start, len);
}

static esp_err_t handle_root_get(httpd_req_t *req) {
  return deui_wifi_portal_send_home(req);
}

static esp_err_t handle_wifi_get(httpd_req_t *req) {
  char prefill_ssid[33] = {0};
  parse_prefill_ssid(req, prefill_ssid, sizeof(prefill_ssid));
  return deui_wifi_portal_send_wifi(req, prefill_ssid);
}

static esp_err_t handle_weight_get(httpd_req_t *req) {
  return deui_wifi_portal_send_weight(req);
}

static esp_err_t handle_updates_get(httpd_req_t *req) {
  return deui_wifi_portal_send_updates(req);
}

static esp_err_t handle_ota_check_post(httpd_req_t *req) {
  esp_err_t err = deui_ota_start_portal_check();
  if (err == ESP_ERR_INVALID_STATE) {
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"started\":false,\"reason\":\"busy\"}", HTTPD_RESP_USE_STRLEN);
  }
  if (err != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to start update check");
  }
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, "{\"started\":true}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_ota_status_get(httpd_req_t *req) {
  char resp[384];
  deui_ota_write_status_json(resp, sizeof(resp));
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_save_post(httpd_req_t *req) {
  char body[256] = {0};
  char ssid[33] = {0};
  char password[65] = {0};
  int read = httpd_req_recv(req, body, sizeof(body) - 1);
  esp_err_t err;

  if (read <= 0) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty payload");
  }
  parse_form_field(body, "ssid", ssid, sizeof(ssid));
  parse_form_field(body, "password", password, sizeof(password));
  if (ssid[0] == '\0') {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID is required");
  }

  err = deui_wifi_settings_save(ssid, password, DEUI_WIFI_DEFAULT_HOSTNAME);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Save settings failed: %s", esp_err_to_name(err));
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save Wi-Fi settings");
  }

  strlcpy(g_deui_wifi.sta_ssid, ssid, sizeof(g_deui_wifi.sta_ssid));
  strlcpy(g_deui_wifi.sta_password, password, sizeof(g_deui_wifi.sta_password));
  strlcpy(g_deui_wifi.info.hostname, DEUI_WIFI_DEFAULT_HOSTNAME, sizeof(g_deui_wifi.info.hostname));
  (void)deui_wifi_mdns_apply_hostname();
  g_deui_wifi.info.has_saved_credentials = true;

  deui_wifi_provision_begin();

  err = deui_wifi_apply_station_config(true);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Set STA config failed: %s", esp_err_to_name(err));
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to apply station config");
  }
  err = deui_wifi_connect_station();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "STA connect start failed: %s", esp_err_to_name(err));
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to start station connect");
  }

  return deui_wifi_portal_send_connecting(req, ssid);
}

static esp_err_t handle_wifi_status_get(httpd_req_t *req) {
  char resp[256];

  deui_wifi_provision_write_status_json(resp, sizeof(resp));
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_reset_post(httpd_req_t *req) {
  char message[220];
  esp_err_t err;

  err = deui_wifi_settings_clear();
  if (err != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to clear saved settings");
  }

  g_deui_wifi.sta_ssid[0] = '\0';
  g_deui_wifi.sta_password[0] = '\0';
  g_deui_wifi.info.has_saved_credentials = false;
  g_deui_wifi.info.sta_connected = false;
  g_deui_wifi.info.sta_connecting = false;
  g_deui_wifi.info.sta_ip[0] = '\0';
  strlcpy(g_deui_wifi.info.hostname, DEUI_WIFI_DEFAULT_HOSTNAME, sizeof(g_deui_wifi.info.hostname));

  (void)snprintf(
      message, sizeof(message),
      "Saved network cleared. Join \"%s\" (password \"%s\"), then open " DEUI_WIFI_PORTAL_URL " or http://"
      DEUI_WIFI_PORTAL_IP "/.",
      g_deui_wifi.info.ap_ssid, g_deui_wifi.info.ap_password);

  /* Send the page before switching Wi-Fi modes so the client receives the response. */
  err = deui_wifi_portal_send_message(req, "Reset complete", message);
  if (err == ESP_OK) {
    deui_wifi_schedule_restore_setup_ap();
  }
  return err;
}

static esp_err_t handle_scan_get(httpd_req_t *req) {
  wifi_scan_config_t scan_cfg = {0};
  wifi_ap_record_t records[16];
  uint16_t count = 0;

  if (esp_wifi_scan_start(&scan_cfg, true) != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Wi-Fi scan failed");
  }
  if (esp_wifi_scan_get_ap_num(&count) != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read scan count");
  }
  if (count > 16) {
    count = 16;
  }
  if (count > 0 && esp_wifi_scan_get_ap_records(&count, records) != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read scan records");
  }
  return deui_wifi_portal_send_scan(req, records, count);
}

esp_err_t deui_wifi_portal_register_handlers(httpd_handle_t server) {
  const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = handle_root_get};
  const httpd_uri_t favicon = {.uri = "/favicon.ico", .method = HTTP_GET, .handler = handle_favicon_get};
  const httpd_uri_t wifi = {.uri = "/wifi", .method = HTTP_GET, .handler = handle_wifi_get};
  const httpd_uri_t weight = {.uri = "/weight", .method = HTTP_GET, .handler = handle_weight_get};
  const httpd_uri_t updates = {.uri = "/updates", .method = HTTP_GET, .handler = handle_updates_get};
  const httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = handle_save_post};
  const httpd_uri_t scan = {.uri = "/scan", .method = HTTP_GET, .handler = handle_scan_get};
  const httpd_uri_t reset = {.uri = "/reset", .method = HTTP_POST, .handler = handle_reset_post};
  const httpd_uri_t stop_weight_get = {
    .uri = "/api/stop-weight",
    .method = HTTP_GET,
    .handler = handle_stop_weight_get,
  };
  const httpd_uri_t stop_weight_post = {
    .uri = "/api/stop-weight",
    .method = HTTP_POST,
    .handler = handle_stop_weight_post,
  };
  const httpd_uri_t wifi_status_get = {
    .uri = "/api/wifi-status",
    .method = HTTP_GET,
    .handler = handle_wifi_status_get,
  };
  const httpd_uri_t ota_check_post = {
    .uri = "/api/ota-check",
    .method = HTTP_POST,
    .handler = handle_ota_check_post,
  };
  const httpd_uri_t ota_status_get = {
    .uri = "/api/ota-status",
    .method = HTTP_GET,
    .handler = handle_ota_status_get,
  };

  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &root), TAG, "Register / failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &favicon), TAG, "Register /favicon.ico failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &wifi), TAG, "Register /wifi failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &weight), TAG, "Register /weight failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &updates), TAG, "Register /updates failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &save), TAG, "Register /save failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &scan), TAG, "Register /scan failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &reset), TAG, "Register /reset failed");
  ESP_RETURN_ON_ERROR(deui_wifi_portal_register_font_handlers(server), TAG, "Register portal fonts failed");
  ESP_RETURN_ON_ERROR(deui_wifi_portal_register_asset_handlers(server), TAG, "Register portal assets failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &stop_weight_get), TAG,
                      "Register GET /api/stop-weight failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &stop_weight_post), TAG,
                      "Register POST /api/stop-weight failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &wifi_status_get), TAG,
                      "Register GET /api/wifi-status failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &ota_check_post), TAG,
                      "Register POST /api/ota-check failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &ota_status_get), TAG,
                      "Register GET /api/ota-status failed");
  return ESP_OK;
}
