#include "wifi_setup.h"

#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "deui_wifi";
static deui_wifi_info_t s_info = {
  .ap_running = false,
  .sta_connected = false,
  .ap_ssid = "DEUI-SETUP",
  .hostname = "deui-controller",
};
static httpd_handle_t s_httpd = NULL;
static bool s_wifi_initialized = false;
static bool s_wifi_suspended = false;

static esp_err_t captive_redirect(httpd_req_t *req) {
  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
  return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handle_root_get(httpd_req_t *req) {
  const char *html =
    "<!doctype html><html><body><h2>DEUI Setup</h2>"
    "<p>If captive portal did not open automatically, use http://192.168.4.1/</p>"
    "<form method='POST' action='/save'>"
    "<label>Wi-Fi SSID</label><br/><input name='ssid'/><br/>"
    "<label>Wi-Fi Password</label><br/><input name='password' type='password'/><br/>"
    "<label>Hostname</label><br/><input name='hostname' value='deui-controller'/><br/>"
    "<button type='submit'>Save</button></form>"
    "<p><a href='/scan'>Scan networks</a></p></body></html>";
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

static void parse_url_field(const char *body, const char *key, char *out, size_t out_size) {
  const char *start = strstr(body, key);
  if (start == NULL || out == NULL || out_size == 0) {
    return;
  }
  start += strlen(key);
  const char *end = strchr(start, '&');
  if (end == NULL) {
    end = start + strlen(start);
  }
  size_t len = (size_t)(end - start);
  if (len >= out_size) {
    len = out_size - 1;
  }
  memcpy(out, start, len);
  out[len] = '\0';
}

static esp_err_t handle_save_post(httpd_req_t *req) {
  char body[256] = {0};
  int read = httpd_req_recv(req, body, sizeof(body) - 1);
  if (read <= 0) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty payload");
  }

  char ssid[33] = {0};
  char password[65] = {0};
  char hostname[33] = {0};
  parse_url_field(body, "ssid=", ssid, sizeof(ssid));
  parse_url_field(body, "password=", password, sizeof(password));
  parse_url_field(body, "hostname=", hostname, sizeof(hostname));

  if (hostname[0] != '\0') {
    strncpy(s_info.hostname, hostname, sizeof(s_info.hostname) - 1);
  }

  wifi_config_t wifi_cfg = {0};
  strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
  strncpy((char *)wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
  ESP_ERROR_CHECK(esp_wifi_connect());

  const char *resp = "Saved. Controller is attempting to join Wi-Fi.";
  return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_scan_get(httpd_req_t *req) {
  wifi_scan_config_t scan_cfg = {0};
  ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_cfg, true));
  uint16_t count = 0;
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&count));
  wifi_ap_record_t records[16];
  if (count > 16) {
    count = 16;
  }
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&count, records));
  char resp[1024];
  size_t used = (size_t)snprintf(resp, sizeof(resp), "DEUI Wi-Fi scan (%u):\n", count);
  for (uint16_t i = 0; i < count && used < sizeof(resp); ++i) {
    used += (size_t)snprintf(resp + used, sizeof(resp) - used, "- %s (%d dBm)\n", records[i].ssid, records[i].rssi);
  }
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data) {
  (void)arg;
  (void)base;
  (void)event_data;
  if (id == WIFI_EVENT_STA_DISCONNECTED) {
    s_info.sta_connected = false;
  } else if (id == WIFI_EVENT_AP_START) {
    s_info.ap_running = true;
  } else if (id == WIFI_EVENT_AP_STOP) {
    s_info.ap_running = false;
  }
}

static void ip_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data) {
  (void)arg;
  (void)base;
  (void)event_data;
  if (id == IP_EVENT_STA_GOT_IP) {
    s_info.sta_connected = true;
  }
}

static esp_err_t start_http(void) {
  if (s_httpd != NULL) {
    return ESP_OK;
  }

  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  ESP_RETURN_ON_ERROR(httpd_start(&s_httpd, &cfg), TAG, "HTTP server start failed");

  const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = handle_root_get};
  const httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = handle_save_post};
  const httpd_uri_t scan = {.uri = "/scan", .method = HTTP_GET, .handler = handle_scan_get};
  const httpd_uri_t android = {.uri = "/generate_204", .method = HTTP_GET, .handler = captive_redirect};
  const httpd_uri_t apple = {.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = captive_redirect};
  const httpd_uri_t ncsi = {.uri = "/ncsi.txt", .method = HTTP_GET, .handler = captive_redirect};
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &root), TAG, "Register root failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &save), TAG, "Register save failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &scan), TAG, "Register scan failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &android), TAG, "Register Android probe failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &apple), TAG, "Register Apple probe failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &ncsi), TAG, "Register Windows probe failed");
  return ESP_OK;
}

static esp_err_t stop_http(void) {
  if (s_httpd == NULL) {
    return ESP_OK;
  }
  esp_err_t err = httpd_stop(s_httpd);
  if (err == ESP_OK) {
    s_httpd = NULL;
    s_info.ap_running = false;
  }
  return err;
}

esp_err_t deui_wifi_init(void) {
  uint8_t mac[6] = {0};
  ESP_RETURN_ON_ERROR(nvs_flash_init(), TAG, "NVS init failed");
  ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "Netif init failed");
  ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "Event loop init failed");
  esp_netif_create_default_wifi_ap();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
  ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "Wi-Fi init failed");

  ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
  snprintf(s_info.ap_ssid, sizeof(s_info.ap_ssid), "DEUI-%02X%02X", mac[4], mac[5]);

  wifi_config_t ap_cfg = {0};
  strncpy((char *)ap_cfg.ap.ssid, s_info.ap_ssid, sizeof(ap_cfg.ap.ssid));
  strncpy((char *)ap_cfg.ap.password, "deui-setup", sizeof(ap_cfg.ap.password));
  ap_cfg.ap.ssid_len = strlen(s_info.ap_ssid);
  ap_cfg.ap.channel = 1;
  ap_cfg.ap.max_connection = 4;
  ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;

  ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL), TAG, "Register Wi-Fi event failed");
  ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL), TAG, "Register IP event failed");
  ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA), TAG, "Set mode failed");
  ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg), TAG, "Set AP config failed");
  ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start failed");
  ESP_RETURN_ON_ERROR(start_http(), TAG, "Start setup HTTP failed");
  s_wifi_initialized = true;
  s_wifi_suspended = false;
  ESP_LOGI(TAG, "DEUI setup AP started: SSID=%s pass=deui-setup url=http://192.168.4.1/", s_info.ap_ssid);
  return ESP_OK;
}

esp_err_t deui_wifi_suspend(void) {
  if (!s_wifi_initialized || s_wifi_suspended) {
    return ESP_OK;
  }

  esp_err_t err = stop_http();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "HTTP stop failed during suspend: %s", esp_err_to_name(err));
  }

  err = esp_wifi_stop();
  if (err != ESP_OK) {
    return err;
  }

  s_info.sta_connected = false;
  s_info.ap_running = false;
  s_wifi_suspended = true;
  ESP_LOGI(TAG, "Wi-Fi suspended");
  return ESP_OK;
}

esp_err_t deui_wifi_resume(void) {
  if (!s_wifi_initialized || !s_wifi_suspended) {
    return ESP_OK;
  }

  ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start failed on resume");
  (void)esp_wifi_connect();
  ESP_RETURN_ON_ERROR(start_http(), TAG, "HTTP restart failed on resume");

  s_wifi_suspended = false;
  ESP_LOGI(TAG, "Wi-Fi resumed");
  return ESP_OK;
}

bool deui_wifi_is_suspended(void) {
  return s_wifi_suspended;
}

void deui_wifi_get_info(deui_wifi_info_t *info) {
  if (info == NULL) {
    return;
  }
  *info = s_info;
}
