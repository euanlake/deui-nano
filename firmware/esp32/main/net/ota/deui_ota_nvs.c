#include "deui_ota_nvs.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "deui_ota_nvs";

#define DEUI_OTA_NVS_NAMESPACE "deui_ota"
#define DEUI_OTA_NVS_KEY_LAST "last_ver"
#define DEUI_OTA_NVS_KEY_ERR "last_err"

esp_err_t deui_ota_nvs_save_last_version(const char *version) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(DEUI_OTA_NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }
  err = nvs_set_str(handle, DEUI_OTA_NVS_KEY_LAST, version != NULL ? version : "");
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }
  nvs_close(handle);
  return err;
}

esp_err_t deui_ota_nvs_save_last_error(const char *message) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(DEUI_OTA_NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }
  err = nvs_set_str(handle, DEUI_OTA_NVS_KEY_ERR, message != NULL ? message : "");
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }
  nvs_close(handle);
  if (err == ESP_OK && message != NULL && message[0] != '\0') {
    ESP_LOGW(TAG, "Recorded OTA error: %s", message);
  }
  return err;
}
