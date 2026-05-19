#include "deui_wifi_portal_fonts.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "deui_wifi_fonts";

extern const uint8_t lab_regular_woff2_start[] asm("_binary_lab_regular_woff2_start");
extern const uint8_t lab_regular_woff2_end[] asm("_binary_lab_regular_woff2_end");
extern const uint8_t lab_medium_woff2_start[] asm("_binary_lab_medium_woff2_start");
extern const uint8_t lab_medium_woff2_end[] asm("_binary_lab_medium_woff2_end");
extern const uint8_t lab_bold_woff2_start[] asm("_binary_lab_bold_woff2_start");
extern const uint8_t lab_bold_woff2_end[] asm("_binary_lab_bold_woff2_end");

typedef struct {
  const char *uri;
  const uint8_t *start;
  const uint8_t *end;
} portal_font_asset_t;

static const portal_font_asset_t k_fonts[] = {
  {"/fonts/lab-regular.woff2", lab_regular_woff2_start, lab_regular_woff2_end},
  {"/fonts/lab-medium.woff2", lab_medium_woff2_start, lab_medium_woff2_end},
  {"/fonts/lab-bold.woff2", lab_bold_woff2_start, lab_bold_woff2_end},
};

static esp_err_t handle_font_get(httpd_req_t *req) {
  const char *uri = req->uri;
  size_t len;

  for (size_t i = 0; i < sizeof(k_fonts) / sizeof(k_fonts[0]); ++i) {
    if (strcmp(uri, k_fonts[i].uri) != 0) {
      continue;
    }
    len = (size_t)(k_fonts[i].end - k_fonts[i].start);
    httpd_resp_set_type(req, "font/woff2");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    return httpd_resp_send(req, (const char *)k_fonts[i].start, len);
  }
  return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Font not found");
}

esp_err_t deui_wifi_portal_register_font_handlers(httpd_handle_t server) {
  static const httpd_uri_t regular = {
    .uri = "/fonts/lab-regular.woff2",
    .method = HTTP_GET,
    .handler = handle_font_get,
  };
  static const httpd_uri_t medium = {
    .uri = "/fonts/lab-medium.woff2",
    .method = HTTP_GET,
    .handler = handle_font_get,
  };
  static const httpd_uri_t bold = {
    .uri = "/fonts/lab-bold.woff2",
    .method = HTTP_GET,
    .handler = handle_font_get,
  };

  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &regular), TAG, "Register regular font failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &medium), TAG, "Register medium font failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &bold), TAG, "Register bold font failed");
  ESP_LOGI(TAG, "Portal fonts: Lab Grotesque regular/medium/bold (woff2)");
  return ESP_OK;
}
