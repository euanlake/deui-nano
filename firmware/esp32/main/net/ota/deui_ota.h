#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

typedef enum {
  DEUI_OTA_TRIGGER_AUTO_SLEEP = 0,
  DEUI_OTA_TRIGGER_PORTAL,
} deui_ota_trigger_t;

typedef enum {
  DEUI_OTA_STATE_IDLE = 0,
  DEUI_OTA_STATE_CHECKING,
  DEUI_OTA_STATE_DOWNLOADING,
  DEUI_OTA_STATE_UP_TO_DATE,
  DEUI_OTA_STATE_NO_INTERNET,
  DEUI_OTA_STATE_FAILED,
  DEUI_OTA_STATE_UPDATING,
} deui_ota_state_t;

esp_err_t deui_ota_init(void);
esp_err_t deui_ota_try_check_and_update(deui_ota_trigger_t trigger);
esp_err_t deui_ota_start_portal_check(void);

bool deui_ota_checked_this_boot(void);
void deui_ota_mark_checked_this_boot(void);
bool deui_ota_is_active(void);
deui_ota_state_t deui_ota_get_state(void);
const char *deui_ota_get_current_version(void);
void deui_ota_write_status_json(char *buf, size_t buf_size);
void deui_ota_tick(int64_t now_us);
