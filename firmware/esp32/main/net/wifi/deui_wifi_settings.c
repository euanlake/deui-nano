#include "deui_wifi_internal.h"

#include <string.h>

#include "esp_check.h"
#include "nvs.h"

static esp_err_t read_string(nvs_handle_t nvs, const char *key, char *dst, size_t dst_size) {
  size_t required = dst_size;
  esp_err_t err;

  if (dst == NULL || dst_size == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  dst[0] = '\0';
  err = nvs_get_str(nvs, key, dst, &required);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    return ESP_OK;
  }
  return err;
}

esp_err_t deui_wifi_settings_load(char *ssid, size_t ssid_size, char *password, size_t password_size, char *hostname,
                                  size_t hostname_size) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open(DEUI_WIFI_NVS_NAMESPACE, NVS_READONLY, &nvs);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    if (ssid != NULL && ssid_size > 0) {
      ssid[0] = '\0';
    }
    if (password != NULL && password_size > 0) {
      password[0] = '\0';
    }
    if (hostname != NULL && hostname_size > 0) {
      strlcpy(hostname, DEUI_WIFI_DEFAULT_HOSTNAME, hostname_size);
    }
    return ESP_OK;
  }
  ESP_RETURN_ON_ERROR(err, "deui_wifi_settings", "Open NVS failed");

  err = read_string(nvs, DEUI_WIFI_NVS_KEY_SSID, ssid, ssid_size);
  if (err == ESP_OK) {
    err = read_string(nvs, DEUI_WIFI_NVS_KEY_PASS, password, password_size);
  }
  if (err == ESP_OK) {
    err = read_string(nvs, DEUI_WIFI_NVS_KEY_HOST, hostname, hostname_size);
    if (hostname != NULL && hostname_size > 0) {
      if (hostname[0] == '\0') {
        strlcpy(hostname, DEUI_WIFI_DEFAULT_HOSTNAME, hostname_size);
      } else if (strcmp(hostname, "deui-controller") == 0) {
        strlcpy(hostname, DEUI_WIFI_DEFAULT_HOSTNAME, hostname_size);
      }
    }
  }
  nvs_close(nvs);
  return err;
}

esp_err_t deui_wifi_settings_save(const char *ssid, const char *password, const char *hostname) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open(DEUI_WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs);
  ESP_RETURN_ON_ERROR(err, "deui_wifi_settings", "Open NVS failed");
  err = nvs_set_str(nvs, DEUI_WIFI_NVS_KEY_SSID, ssid);
  if (err != ESP_OK) {
    goto exit;
  }
  err = nvs_set_str(nvs, DEUI_WIFI_NVS_KEY_PASS, password);
  if (err != ESP_OK) {
    goto exit;
  }
  err = nvs_set_str(nvs, DEUI_WIFI_NVS_KEY_HOST, hostname);
  if (err != ESP_OK) {
    goto exit;
  }
  err = nvs_commit(nvs);
exit:
  nvs_close(nvs);
  return err;
}

esp_err_t deui_wifi_settings_clear(void) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open(DEUI_WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    return ESP_OK;
  }
  ESP_RETURN_ON_ERROR(err, "deui_wifi_settings", "Open NVS failed");
  err = nvs_erase_all(nvs);
  if (err != ESP_OK) {
    goto exit;
  }
  err = nvs_commit(nvs);
exit:
  nvs_close(nvs);
  return err;
}
