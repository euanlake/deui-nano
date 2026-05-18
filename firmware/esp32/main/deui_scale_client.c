#include "deui_scale_client.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "esp_central.h"

static const char *TAG = "deui_scale";

static const char *const k_scale_svc_str = "00000ffe-0000-1000-8000-00805f9b34fb";
static const char *const k_scale_data_chr_str = "0000ff11-0000-1000-8000-00805f9b34fb";
static const char *const k_scale_cmd_chr_str = "0000ff12-0000-1000-8000-00805f9b34fb";

/** Bookoo packet constants (bytes 7-9 = 24-bit magnitude, byte 6 indicates sign). */
enum {
  k_bookoo_packet_len = 20,
  k_bookoo_sign_byte = 6,
  k_bookoo_weight_msb = 7,
  k_bookoo_weight_mid = 8,
  k_bookoo_weight_lsb = 9,
  k_bookoo_battery_byte = 13,
  k_bookoo_negative_ascii = 0x2d,
};

/** Scan / reconnect cadence while DE1 stays connected. */
enum {
  k_scale_rescan_period_us = 3000000,
};

static SemaphoreHandle_t s_scale_mtx;
static deui_scale_status_t s_scale_status;
static bool s_initialized;
static bool s_suspended;

static ble_uuid_any_t s_uuid_scale_svc;
static ble_uuid_any_t s_uuid_scale_data;
static ble_uuid_any_t s_uuid_scale_cmd;

static bool s_connect_pending;
static ble_addr_t s_pending_addr;
static char s_pending_name[32];

static bool s_scale_discovery_pending;
static uint16_t s_scale_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_scale_data_val_handle;
static uint16_t s_scale_cmd_val_handle;
static int64_t s_next_scan_us;
static int64_t s_last_weight_log_us;

static int on_scale_gap_event(struct ble_gap_event *event, void *arg);
static void start_scale_scan(void);
static void connect_pending_scale(void);
static void reset_scale_connection_state(void);
static void on_scale_disc_complete(const struct peer *peer, int status, void *arg);
static int on_scale_cccd_written(uint16_t conn_handle, const struct ble_gatt_error *error,
                                 struct ble_gatt_attr *attr, void *arg);

static const uint8_t k_bookoo_tare_cmd[6] = {0x03, 0x0a, 0x01, 0x00, 0x00, 0x08};

bool deui_scale_parse_bookoo_packet(const uint8_t *packet, uint16_t len, float *weight_g_out,
                                    int *battery_percent_out) {
  if (packet == NULL || len != k_bookoo_packet_len || weight_g_out == NULL ||
      battery_percent_out == NULL) {
    return false;
  }

  uint8_t sign = packet[k_bookoo_sign_byte];
  uint8_t b7 = packet[k_bookoo_weight_msb];
  uint8_t b8 = packet[k_bookoo_weight_mid];
  uint8_t b9 = packet[k_bookoo_weight_lsb];
  int raw_weight = ((int)b7 << 16) | ((int)b8 << 8) | (int)b9;
  if (sign == k_bookoo_negative_ascii) {
    raw_weight = -raw_weight;
  }

  int battery = packet[k_bookoo_battery_byte];
  if (battery < 0) {
    battery = 0;
  } else if (battery > 100) {
    battery = 100;
  }

  *weight_g_out = ((float)raw_weight) / 100.0f;
  *battery_percent_out = battery;
  return true;
}

esp_err_t deui_scale_init(void) {
  if (s_initialized) {
    return ESP_OK;
  }

  s_scale_mtx = xSemaphoreCreateMutex();
  if (s_scale_mtx == NULL) {
    return ESP_ERR_NO_MEM;
  }

  memset(&s_scale_status, 0, sizeof(s_scale_status));
  memset(s_pending_name, 0, sizeof(s_pending_name));

  if (ble_uuid_from_str(&s_uuid_scale_svc, k_scale_svc_str) != 0 ||
      ble_uuid_from_str(&s_uuid_scale_data, k_scale_data_chr_str) != 0 ||
      ble_uuid_from_str(&s_uuid_scale_cmd, k_scale_cmd_chr_str) != 0) {
    ESP_LOGE(TAG, "Invalid BOOKOO UUID constants");
    return ESP_FAIL;
  }

  s_next_scan_us = 0;
  s_suspended = false;
  s_initialized = true;
  ESP_LOGI(TAG, "Scale client initialized (BOOKOO only)");
  return ESP_OK;
}

esp_err_t deui_scale_suspend(void) {
  if (!s_initialized || s_suspended) {
    return ESP_OK;
  }

  s_suspended = true;
  s_connect_pending = false;
  s_scale_discovery_pending = false;
  s_pending_name[0] = '\0';

  if (s_scale_status.scanning) {
    (void)ble_gap_disc_cancel();
  }
  if (s_scale_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
    (void)ble_gap_terminate(s_scale_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    peer_delete(s_scale_conn_handle);
  }
  reset_scale_connection_state();
  ESP_LOGI(TAG, "Scale client suspended");
  return ESP_OK;
}

esp_err_t deui_scale_resume(void) {
  if (!s_initialized || !s_suspended) {
    return ESP_OK;
  }
  s_suspended = false;
  s_next_scan_us = 0;
  ESP_LOGI(TAG, "Scale client resumed");
  return ESP_OK;
}

bool deui_scale_is_suspended(void) {
  return s_suspended;
}

void deui_scale_get_status(deui_scale_status_t *status) {
  if (status == NULL || s_scale_mtx == NULL) {
    return;
  }
  if (xSemaphoreTake(s_scale_mtx, pdMS_TO_TICKS(100)) != pdTRUE) {
    memset(status, 0, sizeof(*status));
    return;
  }
  memcpy(status, &s_scale_status, sizeof(*status));
  xSemaphoreGive(s_scale_mtx);
}

esp_err_t deui_scale_send_tare(void) {
  if (!s_initialized || s_scale_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
    return ESP_ERR_INVALID_STATE;
  }
  if (s_scale_cmd_val_handle == 0) {
    return ESP_ERR_NOT_FOUND;
  }

  ESP_LOGI(TAG, "Scale tare command: sending");
  int rc = ble_gattc_write_flat(s_scale_conn_handle, s_scale_cmd_val_handle, k_bookoo_tare_cmd,
                                sizeof(k_bookoo_tare_cmd), NULL, NULL);
  if (rc != 0) {
    ESP_LOGW(TAG, "Scale tare command failed rc=%d", rc);
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "Scale tare command sent");
  return ESP_OK;
}

void deui_scale_tick(bool allow_scan) {
  if (!s_initialized || !ble_hs_synced()) {
    return;
  }
  if (s_suspended) {
    return;
  }

  if (!allow_scan) {
    if (s_scale_status.scanning) {
      int rc = ble_gap_disc_cancel();
      if (rc != 0) {
        ESP_LOGD(TAG, "scale scan cancel rc=%d", rc);
      }
      if (xSemaphoreTake(s_scale_mtx, pdMS_TO_TICKS(30)) == pdTRUE) {
        s_scale_status.scanning = false;
        xSemaphoreGive(s_scale_mtx);
      }
    }
    return;
  }

  if (s_connect_pending && s_scale_conn_handle == BLE_HS_CONN_HANDLE_NONE &&
      !s_scale_discovery_pending) {
    s_connect_pending = false;
    connect_pending_scale();
    return;
  }

  if (s_scale_conn_handle != BLE_HS_CONN_HANDLE_NONE || s_scale_discovery_pending ||
      s_scale_status.scanning || s_connect_pending) {
    return;
  }

  int64_t now_us = esp_timer_get_time();
  if (s_next_scan_us != 0 && now_us < s_next_scan_us) {
    return;
  }

  start_scale_scan();
}

static bool adv_has_bookoo_service(const struct ble_hs_adv_fields *fields) {
  for (unsigned i = 0; i < fields->num_uuids16; ++i) {
    if (fields->uuids16[i].value == 0x0ffe) {
      return true;
    }
  }

  for (unsigned i = 0; i < fields->num_uuids128; ++i) {
    if (ble_uuid_cmp((const ble_uuid_t *)&s_uuid_scale_svc, &fields->uuids128[i].u) == 0) {
      return true;
    }
  }
  for (unsigned i = 0; i < fields->sol_num_uuids128; ++i) {
    if (ble_uuid_cmp((const ble_uuid_t *)&s_uuid_scale_svc, &fields->sol_uuids128[i].u) == 0) {
      return true;
    }
  }
  return false;
}

static void start_scale_scan(void) {
  uint8_t own_addr_type = 0;
  int rc = ble_hs_id_infer_auto(0, &own_addr_type);
  if (rc != 0) {
    ESP_LOGW(TAG, "scale infer own addr rc=%d", rc);
    return;
  }

  struct ble_gap_disc_params p = {.filter_duplicates = 1, .passive = 0};
  ESP_LOGI(TAG, "Scale search attempt: starting BOOKOO scan window");
  rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &p, on_scale_gap_event, NULL);
  if (rc != 0) {
    ESP_LOGD(TAG, "scale ble_gap_disc rc=%d", rc);
    s_next_scan_us = esp_timer_get_time() + k_scale_rescan_period_us;
    return;
  }

  if (xSemaphoreTake(s_scale_mtx, pdMS_TO_TICKS(30)) == pdTRUE) {
    s_scale_status.scanning = true;
    xSemaphoreGive(s_scale_mtx);
  }
  ESP_LOGI(TAG, "Scale search active: scanning for BOOKOO Theme Mini");
}

static void connect_pending_scale(void) {
  uint8_t own_addr_type = 0;
  int rc = ble_hs_id_infer_auto(0, &own_addr_type);
  if (rc != 0) {
    ESP_LOGW(TAG, "scale infer addr before connect rc=%d", rc);
    s_next_scan_us = esp_timer_get_time() + k_scale_rescan_period_us;
    return;
  }

  ESP_LOGI(TAG, "Scale connect attempt: peer=\"%s\" addr=%s",
           s_pending_name[0] != '\0' ? s_pending_name : "(no name)", addr_str(s_pending_addr.val));

  rc = ble_gap_connect(own_addr_type, &s_pending_addr, 30000, NULL, on_scale_gap_event, NULL);
  if (rc != 0) {
    ESP_LOGW(TAG, "scale ble_gap_connect rc=%d", rc);
    s_next_scan_us = esp_timer_get_time() + k_scale_rescan_period_us;
  }
}

static int on_scale_gap_event(struct ble_gap_event *event, void *arg) {
  (void)arg;

  switch (event->type) {
  case BLE_GAP_EVENT_DISC: {
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) != 0) {
      return 0;
    }
    if (!adv_has_bookoo_service(&fields)) {
      return 0;
    }

    size_t n = 0;
    memset(s_pending_name, 0, sizeof(s_pending_name));
    if (fields.name != NULL && fields.name_len > 0) {
      n = fields.name_len;
      if (n >= sizeof(s_pending_name)) {
        n = sizeof(s_pending_name) - 1;
      }
      memcpy(s_pending_name, fields.name, n);
      while (n > 0 && (s_pending_name[n - 1] == '\0' || isspace((unsigned char)s_pending_name[n - 1]))) {
        n--;
      }
      s_pending_name[n] = '\0';
    }
    if (n == 0) {
      snprintf(s_pending_name, sizeof(s_pending_name), "BOOKOO");
    }

    s_pending_addr = event->disc.addr;
    s_connect_pending = true;
    int rc = ble_gap_disc_cancel();
    if (rc != 0) {
      ESP_LOGD(TAG, "scale disc cancel rc=%d", rc);
    }
    return 0;
  }

  case BLE_GAP_EVENT_DISC_COMPLETE:
    if (xSemaphoreTake(s_scale_mtx, pdMS_TO_TICKS(30)) == pdTRUE) {
      s_scale_status.scanning = false;
      xSemaphoreGive(s_scale_mtx);
    }
    s_next_scan_us = esp_timer_get_time() + k_scale_rescan_period_us;
    return 0;

  case BLE_GAP_EVENT_CONNECT:
    if (event->connect.status != 0) {
      ESP_LOGW(TAG, "Scale connect failed status=%d", event->connect.status);
      reset_scale_connection_state();
      s_next_scan_us = esp_timer_get_time() + k_scale_rescan_period_us;
      return 0;
    }

    s_scale_conn_handle = event->connect.conn_handle;
    if (peer_add(s_scale_conn_handle) != 0) {
      ESP_LOGE(TAG, "peer_add failed for scale conn");
      ble_gap_terminate(s_scale_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
      reset_scale_connection_state();
      return 0;
    }

    s_scale_discovery_pending = true;
    if (peer_disc_svc_by_uuid(s_scale_conn_handle, (const ble_uuid_t *)&s_uuid_scale_svc,
                              on_scale_disc_complete, NULL) != 0) {
      ESP_LOGE(TAG, "Scale service discovery start failed");
      s_scale_discovery_pending = false;
      ble_gap_terminate(s_scale_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
      reset_scale_connection_state();
      return 0;
    }

    if (xSemaphoreTake(s_scale_mtx, pdMS_TO_TICKS(50)) == pdTRUE) {
      s_scale_status.connected = true;
      s_scale_status.scanning = false;
      strncpy(s_scale_status.device_name, s_pending_name, sizeof(s_scale_status.device_name) - 1);
      s_scale_status.device_name[sizeof(s_scale_status.device_name) - 1] = '\0';
      xSemaphoreGive(s_scale_mtx);
    }
    ESP_LOGI(TAG, "Scale connection successful: discovering BOOKOO characteristics");
    return 0;

  case BLE_GAP_EVENT_NOTIFY_RX: {
    if (event->notify_rx.conn_handle != s_scale_conn_handle || s_scale_data_val_handle == 0 ||
        event->notify_rx.attr_handle != s_scale_data_val_handle) {
      return 0;
    }

    uint8_t packet[k_bookoo_packet_len];
    uint16_t raw_len = OS_MBUF_PKTLEN(event->notify_rx.om);
    if (raw_len != k_bookoo_packet_len) {
      return 0;
    }
    if (os_mbuf_copydata(event->notify_rx.om, 0, raw_len, packet) != 0) {
      return 0;
    }

    float weight_g = 0.0f;
    int battery = 0;
    if (!deui_scale_parse_bookoo_packet(packet, raw_len, &weight_g, &battery)) {
      return 0;
    }

    if (xSemaphoreTake(s_scale_mtx, pdMS_TO_TICKS(30)) == pdTRUE) {
      s_scale_status.has_weight = true;
      s_scale_status.weight_g = weight_g;
      s_scale_status.battery_percent = battery;
      s_scale_status.last_update_us = esp_timer_get_time();
      xSemaphoreGive(s_scale_mtx);
    }
    if (s_scale_status.last_update_us - s_last_weight_log_us >= 2000000) {
      s_last_weight_log_us = s_scale_status.last_update_us;
      ESP_LOGI(TAG, "Scale weight update: %.2fg (battery %d%%)", weight_g, battery);
    }
    return 0;
  }

  case BLE_GAP_EVENT_DISCONNECT:
    if (event->disconnect.conn.conn_handle == s_scale_conn_handle) {
      ESP_LOGI(TAG, "Scale disconnected status=%d", event->disconnect.reason);
      peer_delete(event->disconnect.conn.conn_handle);
      reset_scale_connection_state();
      s_next_scan_us = esp_timer_get_time() + k_scale_rescan_period_us;
    }
    return 0;

  default:
    return 0;
  }
}

static void on_scale_disc_complete(const struct peer *peer, int status, void *arg) {
  (void)arg;
  s_scale_discovery_pending = false;

  if (status != 0 || peer == NULL) {
    ESP_LOGE(TAG, "Scale GATT discovery failed status=%d", status);
    if (peer != NULL) {
      ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
      peer_delete(peer->conn_handle);
    }
    reset_scale_connection_state();
    return;
  }

  const struct peer_chr *data_chr =
      peer_chr_find_uuid(peer, (const ble_uuid_t *)&s_uuid_scale_svc, (const ble_uuid_t *)&s_uuid_scale_data);
  if (data_chr == NULL) {
    ESP_LOGE(TAG, "BOOKOO data characteristic missing");
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    peer_delete(peer->conn_handle);
    reset_scale_connection_state();
    return;
  }

  const struct peer_dsc *cccd = peer_dsc_find_uuid(peer, (const ble_uuid_t *)&s_uuid_scale_svc,
                                                    (const ble_uuid_t *)&s_uuid_scale_data,
                                                    BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
  if (cccd == NULL) {
    ESP_LOGE(TAG, "BOOKOO CCCD missing");
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    peer_delete(peer->conn_handle);
    reset_scale_connection_state();
    return;
  }

  s_scale_data_val_handle = data_chr->chr.val_handle;

  const struct peer_chr *cmd_chr =
      peer_chr_find_uuid(peer, (const ble_uuid_t *)&s_uuid_scale_svc, (const ble_uuid_t *)&s_uuid_scale_cmd);
  if (cmd_chr == NULL) {
    ESP_LOGW(TAG, "BOOKOO command characteristic missing (tare unavailable)");
    s_scale_cmd_val_handle = 0;
  } else {
    s_scale_cmd_val_handle = cmd_chr->chr.val_handle;
  }

  uint8_t notify_on[2] = {1, 0};
  int rc = ble_gattc_write_flat(peer->conn_handle, cccd->dsc.handle, notify_on, sizeof(notify_on),
                                on_scale_cccd_written, NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "BOOKOO notify enable failed rc=%d", rc);
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    peer_delete(peer->conn_handle);
    reset_scale_connection_state();
    return;
  }
}

static int on_scale_cccd_written(uint16_t conn_handle, const struct ble_gatt_error *error,
                                 struct ble_gatt_attr *attr, void *arg) {
  (void)conn_handle;
  (void)attr;
  (void)arg;

  if (error != NULL && error->status != 0) {
    ESP_LOGE(TAG, "BOOKOO CCCD write failed status=%d", error->status);
    if (s_scale_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
      ble_gap_terminate(s_scale_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    reset_scale_connection_state();
    return 0;
  }

  ESP_LOGI(TAG, "BOOKOO notifications enabled");
  return 0;
}

static void reset_scale_connection_state(void) {
  s_scale_conn_handle = BLE_HS_CONN_HANDLE_NONE;
  s_scale_data_val_handle = 0;
  s_scale_cmd_val_handle = 0;
  s_scale_discovery_pending = false;
  s_connect_pending = false;
  s_last_weight_log_us = 0;

  if (xSemaphoreTake(s_scale_mtx, pdMS_TO_TICKS(50)) == pdTRUE) {
    s_scale_status.connected = false;
    s_scale_status.scanning = false;
    s_scale_status.has_weight = false;
    s_scale_status.weight_g = 0.0f;
    s_scale_status.battery_percent = 0;
    s_scale_status.last_update_us = 0;
    s_scale_status.device_name[0] = '\0';
    xSemaphoreGive(s_scale_mtx);
  }
}
