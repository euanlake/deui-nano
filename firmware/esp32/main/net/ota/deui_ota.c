#include "deui_ota.h"
#include "deui_ota_policy.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_app_format.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "cJSON.h"

static const char *TAG = "deui_ota";

#define DEUI_OTA_BOOT_HEALTH_US (30LL * 1000000LL)

static deui_ota_state_t s_state = DEUI_OTA_STATE_IDLE;
static int64_t s_boot_us = 0;
static bool s_mark_valid_done = false;
static bool s_checked_this_boot = false;
static bool s_active = false;
static char s_remote_version[32] = {0};
static char s_status_message[96] = {0};
static TaskHandle_t s_portal_task = NULL;

typedef struct {
  char version[32];
  char url[256];
  char sha256[65];
} deui_ota_manifest_t;

static void set_state(deui_ota_state_t state, const char *message) {
  s_state = state;
  if (message != NULL) {
    strlcpy(s_status_message, message, sizeof(s_status_message));
  }
}

const char *deui_ota_get_current_version(void) {
  const esp_app_desc_t *desc = esp_app_get_description();
  return (desc != NULL && desc->version[0] != '\0') ? desc->version : "0.0.0";
}

static int parse_version_part(const char **cursor) {
  const char *p = *cursor;
  int value = 0;
  if (p == NULL || !isdigit((unsigned char)*p)) {
    return -1;
  }
  while (*p != '\0' && isdigit((unsigned char)*p)) {
    value = value * 10 + (*p - '0');
    p++;
  }
  *cursor = p;
  return value;
}

static int semver_compare(const char *a, const char *b) {
  const char *pa = a;
  const char *pb = b;
  for (int i = 0; i < 3; i++) {
    int va = parse_version_part(&pa);
    int vb = parse_version_part(&pb);
    if (va < 0 || vb < 0) {
      return strcmp(a, b);
    }
    if (va != vb) {
      return (va < vb) ? -1 : 1;
    }
    if (i < 2) {
      if (*pa != '.') {
        return (*pa == '\0') ? -1 : 1;
      }
      if (*pb != '.') {
        return (*pb == '\0') ? 1 : -1;
      }
      pa++;
      pb++;
    }
  }
  return 0;
}

static esp_err_t http_get_to_buffer(const char *url, char *out, size_t out_size) {
  esp_http_client_config_t config = {
    .url = url,
    .crt_bundle_attach = esp_crt_bundle_attach,
    .timeout_ms = 15000,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    esp_http_client_cleanup(client);
    return err;
  }

  (void)esp_http_client_fetch_headers(client);
  size_t total = 0;
  while (total + 1 < out_size) {
    const int read = esp_http_client_read(client, out + total, (int)(out_size - 1 - total));
    if (read < 0) {
      esp_http_client_close(client);
      esp_http_client_cleanup(client);
      return ESP_FAIL;
    }
    if (read == 0) {
      break;
    }
    total += (size_t)read;
  }
  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  if (total == 0) {
    return ESP_FAIL;
  }
  out[total] = '\0';
  return ESP_OK;
}

static bool parse_manifest(const char *json, deui_ota_manifest_t *manifest) {
  cJSON *root = cJSON_Parse(json);
  if (root == NULL) {
    return false;
  }

  const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
  const cJSON *url = cJSON_GetObjectItemCaseSensitive(root, "url");
  const cJSON *sha256 = cJSON_GetObjectItemCaseSensitive(root, "sha256");
  const cJSON *product = cJSON_GetObjectItemCaseSensitive(root, "product");

  if (!cJSON_IsString(version) || !cJSON_IsString(url)) {
    cJSON_Delete(root);
    return false;
  }
  if (cJSON_IsString(product) && strcmp(product->valuestring, "deui_nano") != 0) {
    cJSON_Delete(root);
    return false;
  }

  strlcpy(manifest->version, version->valuestring, sizeof(manifest->version));
  strlcpy(manifest->url, url->valuestring, sizeof(manifest->url));
  if (cJSON_IsString(sha256)) {
    strlcpy(manifest->sha256, sha256->valuestring, sizeof(manifest->sha256));
  } else {
    manifest->sha256[0] = '\0';
  }

  cJSON_Delete(root);
  return true;
}

static esp_err_t fetch_manifest(deui_ota_manifest_t *manifest) {
  char body[512] = {0};
  esp_err_t err = http_get_to_buffer(CONFIG_DEUI_OTA_MANIFEST_URL, body, sizeof(body));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Manifest fetch failed: %s", esp_err_to_name(err));
    return err;
  }
  if (!parse_manifest(body, manifest)) {
    ESP_LOGE(TAG, "Manifest parse failed");
    return ESP_ERR_INVALID_RESPONSE;
  }
  return ESP_OK;
}

static esp_err_t download_firmware(const deui_ota_manifest_t *manifest) {
  esp_http_client_config_t config = {
    .url = manifest->url,
    .crt_bundle_attach = esp_crt_bundle_attach,
    .timeout_ms = 60000,
    .keep_alive_enable = true,
  };
  esp_https_ota_config_t ota_config = {
    .http_config = &config,
  };

  set_state(DEUI_OTA_STATE_DOWNLOADING, "Downloading firmware...");
  esp_err_t err = esp_https_ota(&ota_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTPS OTA failed: %s", esp_err_to_name(err));
    return err;
  }

  if (manifest->sha256[0] != '\0') {
    ESP_LOGI(TAG, "Manifest sha256=%s (verified at release by CI)", manifest->sha256);
  }

  set_state(DEUI_OTA_STATE_UPDATING, "Installing update. Device will restart...");
  ESP_LOGI(TAG, "OTA complete, restarting");
  esp_restart();
  return ESP_OK;
}

esp_err_t deui_ota_init(void) {
#if !CONFIG_DEUI_OTA_ENABLE
  return ESP_OK;
#else
  s_boot_us = esp_timer_get_time();
  ESP_LOGI(TAG, "Running firmware version %s", deui_ota_get_current_version());
  return ESP_OK;
#endif
}

void deui_ota_tick(int64_t now_us) {
#if CONFIG_DEUI_OTA_ENABLE
  if (s_mark_valid_done || now_us - s_boot_us < DEUI_OTA_BOOT_HEALTH_US) {
    return;
  }

  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;
  if (running == NULL || esp_ota_get_state_partition(running, &ota_state) != ESP_OK) {
    return;
  }
  if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
    ESP_LOGI(TAG, "Marking OTA image valid after boot health check");
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
      s_mark_valid_done = true;
    }
  } else {
    s_mark_valid_done = true;
  }
#else
  (void)now_us;
#endif
}

bool deui_ota_checked_this_boot(void) {
  return s_checked_this_boot;
}

void deui_ota_mark_checked_this_boot(void) {
  s_checked_this_boot = true;
}

bool deui_ota_is_active(void) {
  return s_active;
}

deui_ota_state_t deui_ota_get_state(void) {
  return s_state;
}

void deui_ota_write_status_json(char *buf, size_t buf_size) {
  const char *phase = "idle";
  switch (s_state) {
  case DEUI_OTA_STATE_CHECKING:
    phase = "checking";
    break;
  case DEUI_OTA_STATE_DOWNLOADING:
    phase = "updating";
    break;
  case DEUI_OTA_STATE_UP_TO_DATE:
    phase = "up_to_date";
    break;
  case DEUI_OTA_STATE_NO_INTERNET:
    phase = "no_internet";
    break;
  case DEUI_OTA_STATE_FAILED:
    phase = "failed";
    break;
  case DEUI_OTA_STATE_UPDATING:
    phase = "updating";
    break;
  default:
    phase = "idle";
    break;
  }

  (void)snprintf(buf, buf_size,
                 "{\"phase\":\"%s\",\"current_version\":\"%s\",\"remote_version\":\"%s\","
                 "\"message\":\"%s\",\"active\":%s}",
                 phase, deui_ota_get_current_version(), s_remote_version, s_status_message,
                 s_active ? "true" : "false");
}

static esp_err_t run_ota_check(deui_ota_trigger_t trigger) {
#if !CONFIG_DEUI_OTA_ENABLE
  (void)trigger;
  return ESP_ERR_NOT_SUPPORTED;
#else
  if (s_active) {
    return ESP_ERR_INVALID_STATE;
  }

  if (deui_ota_policy_should_defer(trigger)) {
    if (trigger == DEUI_OTA_TRIGGER_PORTAL) {
      set_state(DEUI_OTA_STATE_FAILED, "Cannot update right now. Try again when idle.");
      return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
  }

  if (!deui_ota_policy_can_download(trigger)) {
    if (trigger == DEUI_OTA_TRIGGER_PORTAL) {
      if (!deui_ota_has_internet()) {
        set_state(DEUI_OTA_STATE_NO_INTERNET, "Connect to home Wi-Fi to check for updates.");
      } else {
        set_state(DEUI_OTA_STATE_FAILED, "Cannot update right now. Try again when idle.");
      }
      return ESP_ERR_INVALID_STATE;
    }
    return ESP_ERR_INVALID_STATE;
  }

  s_active = true;
  set_state(DEUI_OTA_STATE_CHECKING, "Checking for updates...");

  deui_ota_manifest_t manifest = {0};
  esp_err_t err = fetch_manifest(&manifest);
  if (err != ESP_OK) {
    set_state(DEUI_OTA_STATE_FAILED, "Update check failed. Try again later.");
    s_active = false;
    return err;
  }

  strlcpy(s_remote_version, manifest.version, sizeof(s_remote_version));
  const int cmp = semver_compare(deui_ota_get_current_version(), manifest.version);
  if (cmp >= 0) {
    set_state(DEUI_OTA_STATE_UP_TO_DATE, "You are on the latest version.");
    s_active = false;
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Update available: %s -> %s", deui_ota_get_current_version(), manifest.version);
  err = download_firmware(&manifest);
  s_active = false;
  return err;
#endif
}

esp_err_t deui_ota_try_check_and_update(deui_ota_trigger_t trigger) {
#if !CONFIG_DEUI_OTA_ENABLE
  (void)trigger;
  return ESP_ERR_NOT_SUPPORTED;
#else
  const esp_err_t err = run_ota_check(trigger);
  if (trigger == DEUI_OTA_TRIGGER_AUTO_SLEEP) {
    deui_ota_mark_checked_this_boot();
  }
  return err;
#endif
}

static void portal_ota_task(void *arg) {
  (void)arg;
  (void)run_ota_check(DEUI_OTA_TRIGGER_PORTAL);
  s_portal_task = NULL;
  vTaskDelete(NULL);
}

esp_err_t deui_ota_start_portal_check(void) {
#if !CONFIG_DEUI_OTA_ENABLE
  return ESP_ERR_NOT_SUPPORTED;
#else
  if (s_portal_task != NULL || s_active) {
    return ESP_ERR_INVALID_STATE;
  }
  if (xTaskCreate(portal_ota_task, "deui_ota_portal", 12288, NULL, 5, &s_portal_task) != pdPASS) {
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
#endif
}
