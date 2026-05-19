#include "deui_wifi_portal_assets.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "deui_wifi_assets";

extern const uint8_t plus_light_png_start[] asm("_binary_plus_light_png_start");
extern const uint8_t plus_light_png_end[] asm("_binary_plus_light_png_end");
extern const uint8_t plus_dark_png_start[] asm("_binary_plus_dark_png_start");
extern const uint8_t plus_dark_png_end[] asm("_binary_plus_dark_png_end");
extern const uint8_t minus_light_png_start[] asm("_binary_minus_light_png_start");
extern const uint8_t minus_light_png_end[] asm("_binary_minus_light_png_end");
extern const uint8_t minus_dark_png_start[] asm("_binary_minus_dark_png_start");
extern const uint8_t minus_dark_png_end[] asm("_binary_minus_dark_png_end");
extern const uint8_t deui_logo_png_start[] asm("_binary_deui_logo_png_start");
extern const uint8_t deui_logo_png_end[] asm("_binary_deui_logo_png_end");

typedef struct {
  const char *uri;
  const uint8_t *start;
  const uint8_t *end;
} portal_png_asset_t;

static const portal_png_asset_t k_pngs[] = {
  {"/img/plus-light.png", plus_light_png_start, plus_light_png_end},
  {"/img/plus-dark.png", plus_dark_png_start, plus_dark_png_end},
  {"/img/minus-light.png", minus_light_png_start, minus_light_png_end},
  {"/img/minus-dark.png", minus_dark_png_start, minus_dark_png_end},
  {"/img/deui-logo.png", deui_logo_png_start, deui_logo_png_end},
};

static esp_err_t handle_png_get(httpd_req_t *req) {
  const char *uri = req->uri;

  for (size_t i = 0; i < sizeof(k_pngs) / sizeof(k_pngs[0]); ++i) {
    if (strcmp(uri, k_pngs[i].uri) != 0) {
      continue;
    }
    const size_t len = (size_t)(k_pngs[i].end - k_pngs[i].start);
    httpd_resp_set_type(req, "image/png");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    return httpd_resp_send(req, (const char *)k_pngs[i].start, len);
  }
  return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Image not found");
}

esp_err_t deui_wifi_portal_register_asset_handlers(httpd_handle_t server) {
  static const httpd_uri_t plus_light = {
    .uri = "/img/plus-light.png",
    .method = HTTP_GET,
    .handler = handle_png_get,
  };
  static const httpd_uri_t plus_dark = {
    .uri = "/img/plus-dark.png",
    .method = HTTP_GET,
    .handler = handle_png_get,
  };
  static const httpd_uri_t minus_light = {
    .uri = "/img/minus-light.png",
    .method = HTTP_GET,
    .handler = handle_png_get,
  };
  static const httpd_uri_t minus_dark = {
    .uri = "/img/minus-dark.png",
    .method = HTTP_GET,
    .handler = handle_png_get,
  };
  static const httpd_uri_t logo = {
    .uri = "/img/deui-logo.png",
    .method = HTTP_GET,
    .handler = handle_png_get,
  };

  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &plus_light), TAG, "plus-light failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &plus_dark), TAG, "plus-dark failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &minus_light), TAG, "minus-light failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &minus_dark), TAG, "minus-dark failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &logo), TAG, "logo failed");
  return ESP_OK;
}
