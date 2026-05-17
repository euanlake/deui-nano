#include "deui_ble_client.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_store.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"

#include "esp_central.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"

void ble_store_config_init(void);

static const char *TAG = "deui_ble";

/** DE1 BLE service UUIDs (canonical string form consumed by NimBLE). */
static const char *const k_svc_str = DE1_SERVICE_UUID;
static const char *const k_shot_chr_str = DE1_CHAR_SHOT_SAMPLE;
static const char *const k_state_chr_str = DE1_CHAR_STATE_INFO;
static const char *const k_requested_state_chr_str = DE1_CHAR_REQUESTED_STATE;

/** Parsed once at NimBLE sync. */
static ble_uuid_any_t s_uuid_svc;
static ble_uuid_any_t s_uuid_shot;
static ble_uuid_any_t s_uuid_state;
static ble_uuid_any_t s_uuid_requested_state;

/** Session / telemetry */
static SemaphoreHandle_t s_status_mtx;
static const uint8_t k_zero_mac[6] = {0};
static deui_ble_status_t s_live;
static char s_remote_name_snapshot[48];
static volatile bool s_nimble_ready;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static volatile bool s_gatt_ready;
static uint16_t s_shot_val_handle;
static uint16_t s_state_val_handle;
static uint16_t s_requested_state_val_handle;
static bool s_discovery_pending;
/** Connect deferred out of GAP scan callback (NimBLE host thread). */
static bool s_connect_pending;
static ble_addr_t s_pending_peer_addr;
/** Last GATT read of DE1 StateInfo (conn_handle valid and discovery done). */
static int64_t s_last_state_read_us;
static bool s_scale_connected;
static bool s_scale_has_weight;
static float s_scale_weight_g;

/** DE1-friendly link parameters (matches legacy DEUI / NimBLE-Arduino client). */
static const struct ble_gap_conn_params k_de1_conn_params = {
    .scan_itvl = 0x0010,
    .scan_window = 0x0010,
    .itvl_min = 24,
    .itvl_max = 48,
    .latency = 0,
    .supervision_timeout = 200,
    .min_ce_len = 0,
    .max_ce_len = 0,
};
/** Monotonic scan telemetry (GAP thread updates; tick reads without mutex). */
static volatile uint32_t s_gap_adv_reports_total;
static volatile uint32_t s_gap_adv_de1_matches;

static void host_task(void *param);
static void on_reset(int reason);
static void on_sync(void);
static int gap_event(struct ble_gap_event *event, void *arg);
static void subscribe_to_state(const struct peer *peer);
static void subscribe_to_shot(const struct peer *peer);
static void on_disc_complete(const struct peer *peer, int status, void *arg);

/** Protocol names from `docs/de1-bluetooth-protocol.md` (Major States / Minor / Error minors). */
static void de1_major_state_name_for_log(uint8_t v, char *out, size_t cap) {
  const char *n = NULL;
  switch (v) {
  case 0x00:
    n = "Sleep";
    break;
  case 0x01:
    n = "GoingToSleep";
    break;
  case 0x02:
    n = "Idle";
    break;
  case 0x03:
    n = "Busy";
    break;
  case 0x04:
    n = "Espresso";
    break;
  case 0x05:
    n = "Steam";
    break;
  case 0x06:
    n = "HotWater";
    break;
  case 0x07:
    n = "ShortCal";
    break;
  case 0x08:
    n = "SelfTest";
    break;
  case 0x09:
    n = "LongCal";
    break;
  case 0x0a:
    n = "Descale";
    break;
  case 0x0b:
    n = "FatalError";
    break;
  case 0x0c:
    n = "Init";
    break;
  case 0x0d:
    n = "NoRequest";
    break;
  case 0x0e:
    n = "SkipToNext";
    break;
  case 0x0f:
    n = "HotWaterRinse";
    break;
  case 0x10:
    n = "SteamRinse";
    break;
  case 0x11:
    n = "Refill";
    break;
  case 0x12:
    n = "Clean";
    break;
  case 0x13:
    n = "InBootLoader";
    break;
  case 0x14:
    n = "AirPurge";
    break;
  case 0x15:
    n = "ScheduledWake";
    break;
  default:
    break;
  }
  if (n != NULL) {
    snprintf(out, cap, "%s", n);
  } else {
    snprintf(out, cap, "unknown(0x%02x)", v);
  }
}

static void de1_minor_state_name_for_log(uint8_t v, char *out, size_t cap) {
  const char *n = NULL;
  switch (v) {
  case 0x00:
    n = "NoState";
    break;
  case 0x01:
    n = "HeatWaterTank";
    break;
  case 0x02:
    n = "HeatWaterHeater";
    break;
  case 0x03:
    n = "StabilizeMixTemp";
    break;
  case 0x04:
    n = "PreInfuse";
    break;
  case 0x05:
    n = "Pour";
    break;
  case 0x06:
    n = "Flush";
    break;
  case 0x07:
    n = "Steaming";
    break;
  case 0x08:
    n = "DescaleInit";
    break;
  case 0x09:
    n = "DescaleFillGroup";
    break;
  case 0x0a:
    n = "DescaleReturn";
    break;
  case 0x0b:
    n = "DescaleGroup";
    break;
  case 0x0c:
    n = "DescaleSteam";
    break;
  case 0x0d:
    n = "CleanInit";
    break;
  case 0x0e:
    n = "CleanFillGroup";
    break;
  case 0x0f:
    n = "CleanSoak";
    break;
  case 0x10:
    n = "CleanGroup";
    break;
  case 0x11:
    n = "PausedRefill";
    break;
  case 0x12:
    n = "PausedSteam";
    break;
  case 0x13:
    n = "UserNotPresent";
    break;
  case 0x14:
    n = "SteamPuff";
    break;
  case 0xc8:
    n = "Error_NaN";
    break;
  case 0xc9:
    n = "Error_Inf";
    break;
  case 0xca:
    n = "Error_Generic";
    break;
  case 0xcb:
    n = "Error_ACC";
    break;
  case 0xcc:
    n = "Error_TSensor";
    break;
  case 0xcd:
    n = "Error_PSensor";
    break;
  case 0xce:
    n = "Error_WLevel";
    break;
  case 0xcf:
    n = "Error_DIP";
    break;
  case 0xd0:
    n = "Error_Assertion";
    break;
  case 0xd1:
    n = "Error_Unsafe";
    break;
  case 0xd2:
    n = "Error_InvalidParm";
    break;
  case 0xd3:
    n = "Error_Flash";
    break;
  case 0xd4:
    n = "Error_OOM";
    break;
  case 0xd5:
    n = "Error_Deadline";
    break;
  case 0xd6:
    n = "Error_HiCurrent";
    break;
  case 0xd7:
    n = "Error_LoCurrent";
    break;
  case 0xd8:
    n = "Error_BootFill";
    break;
  default:
    break;
  }
  if (n != NULL) {
    snprintf(out, cap, "%s", n);
  } else {
    snprintf(out, cap, "unknown(0x%02x)", v);
  }
}

/** Caller must hold `s_status_mtx`. Emits once per DE1 StateInfo transition from `de1_apply_state_bytes_unlocked`. */
static void log_ble_de1_state_unlocked(void) {
  char maj[48];
  char min[56];
  char peer[sizeof(s_remote_name_snapshot)];
  uint8_t addr[6];
  bool st_ok;
  uint8_t ma;
  uint8_t mi;

  ma = s_live.de1_major_state;
  mi = s_live.de1_minor_state;
  st_ok = s_live.de1_state_valid;
  memcpy(addr, s_live.peer_addr_be, sizeof addr);
  strncpy(peer, s_remote_name_snapshot, sizeof peer);
  peer[sizeof peer - 1] = '\0';

  de1_major_state_name_for_log(ma, maj, sizeof maj);
  de1_minor_state_name_for_log(mi, min, sizeof min);

  ESP_LOGI(
      TAG,
      "BLE: DE1 state peer=\"%s\" addr=%02x:%02x:%02x:%02x:%02x:%02x major=%s minor=%s state_valid=%d",
      peer, addr[5], addr[4], addr[3], addr[2], addr[1], addr[0], maj, min, (int)st_ok);
}

/** Caller must hold `s_status_mtx`. */
static bool de1_shot_time_should_show_unlocked(void) {
  if (!s_live.de1_state_valid) {
    return false;
  }
  /** DE1 is in Espresso major state — shot sample stream may be shown. */
  return s_live.de1_major_state == DE1_MAJOR_STATE_ESPRESSO;
}

/** Caller must hold `s_status_mtx`. */
static void de1_format_machine_state_label_unlocked(void) {
  uint8_t maj = s_live.de1_major_state;
  uint8_t min = s_live.de1_minor_state;
  const char *s = "-";

  switch (maj) {
  case 0x00:
  case 0x01:
    s = "Sleep";
    break;
  case 0x02:
    if (min == 0x00) {
      s = "Ready";
    } else if (min == 0x01 || min == 0x02 || min == 0x03) {
      s = "Heating";
    } else {
      s = "Idle";
    }
    break;
  case 0x03:
    s = "Busy";
    break;
  case 0x04:
    s = "Espresso";
    break;
  case 0x05:
    s = "Steam";
    break;
  case 0x06:
    s = "Hot water";
    break;
  case 0x07:
    s = "Calibrate";
    break;
  case 0x08:
    s = "Self test";
    break;
  case 0x09:
    s = "Calibrate";
    break;
  case 0x0a:
    s = "Descale";
    break;
  case 0x0b:
    s = "Error";
    break;
  case 0x0c:
    s = "Init";
    break;
  case 0x0f:
    s = "Rinse";
    break;
  case 0x10:
    s = "Steam rinse";
    break;
  case 0x11:
    s = "Refill";
    break;
  case 0x12:
    s = "Clean";
    break;
  case 0x13:
    s = "Bootloader";
    break;
  case 0x14:
    s = "Air purge";
    break;
  case 0x15:
    s = "Wake";
    break;
  default:
    s = "Idle";
    break;
  }

  const size_t lbl_cap = sizeof(s_live.machine_state_label) - 1;
  const char *nm = s_remote_name_snapshot;
  while (nm[0] != '\0' && isspace((unsigned char)nm[0])) {
    nm++;
  }
  bool have_peer = nm[0] != '\0' && strcmp(nm, "(no name)") != 0;
  if (!have_peer) {
    strncpy(s_live.machine_state_label, s, lbl_cap);
    s_live.machine_state_label[lbl_cap] = '\0';
    return;
  }

  /** "Name · State" for idle/connected headline; shorten name until the pair fits (~24 chars). */
  size_t name_len = strlen(nm);
  while (name_len > 3) {
    int nprinted = snprintf(s_live.machine_state_label, sizeof(s_live.machine_state_label), "%.*s · %s",
                            (int)name_len, nm, s);
    if (nprinted >= 0 && (size_t)nprinted < sizeof(s_live.machine_state_label)) {
      return;
    }
    name_len--;
  }
  strncpy(s_live.machine_state_label, s, lbl_cap);
  s_live.machine_state_label[lbl_cap] = '\0';
}

/** Caller must hold `s_status_mtx`. */
static void de1_apply_state_bytes_unlocked(uint8_t major, uint8_t minor) {
  bool had_valid = s_live.de1_state_valid;
  uint8_t prev_maj = s_live.de1_major_state;
  uint8_t prev_min = s_live.de1_minor_state;

  s_live.de1_state_valid = true;
  s_live.de1_major_state = major;
  s_live.de1_minor_state = minor;
  de1_format_machine_state_label_unlocked();

  if (!had_valid || prev_maj != major || prev_min != minor) {
    log_ble_de1_state_unlocked();
  }

  if (!de1_shot_time_should_show_unlocked()) {
    /** Drop stale shot-sample presentation while idle so the next notify cannot flash junk. */
    s_live.has_live_data = false;
    s_live.weight_g = 0.f;
    s_live.shot_time_s = 0.f;
    s_live.flow_ml_s = 0.f;
    s_live.pressure_bar = 0.f;
  }
}

static int on_state_gatt_read(uint16_t conn_handle, const struct ble_gatt_error *error,
                              struct ble_gatt_attr *attr, void *arg);
static void read_de1_state_now(uint16_t conn_handle);

static void status_reset_disconnected(bool scanning) {
  if (xSemaphoreTake(s_status_mtx, portMAX_DELAY) != pdTRUE) {
    return;
  }
  memset(&s_live, 0, sizeof(s_live));
  s_live.scanning = scanning;
  s_state_val_handle = 0;
  s_requested_state_val_handle = 0;
  s_last_state_read_us = 0;
  s_scale_connected = false;
  s_scale_has_weight = false;
  s_scale_weight_g = 0.f;
  if (scanning) {
    strncpy(s_live.detail_line, "Scanning for DE1…", sizeof(s_live.detail_line) - 1);
    strncpy(s_live.ble_heading, "Bluetooth: idle", sizeof(s_live.ble_heading) - 1);
  } else {
    strncpy(s_live.detail_line, "", sizeof(s_live.detail_line));
    strncpy(s_live.ble_heading, "Bluetooth: standby", sizeof(s_live.ble_heading) - 1);
  }
  xSemaphoreGive(s_status_mtx);
}

static void locked_heading_detail_scan(bool scanning, const char *heading, const char *detail) {
  if (xSemaphoreTake(s_status_mtx, portMAX_DELAY) != pdTRUE) {
    return;
  }
  if (heading != NULL) {
    strncpy(s_live.ble_heading, heading, sizeof(s_live.ble_heading) - 1);
    s_live.ble_heading[sizeof(s_live.ble_heading) - 1] = '\0';
  }
  if (detail != NULL) {
    strncpy(s_live.detail_line, detail, sizeof(s_live.detail_line) - 1);
    s_live.detail_line[sizeof(s_live.detail_line) - 1] = '\0';
  }
  s_live.scanning = scanning;
  xSemaphoreGive(s_status_mtx);
}

/** Caller must hold `s_status_mtx`. */
static bool telemetry_apply_unlocked(const uint8_t *payload, uint16_t len) {
  de1_shot_sample_t sample = {0};
  if (!deui_ble_parse_shot_sample(payload, len, &sample)) {
    return false;
  }

  float t_s = (float)sample.sample_time / 1000.0f;
  float flow = sample.group_flow;
  float group_pressure_bar = sample.group_pressure;

  s_live.pressure_bar = group_pressure_bar;
  s_live.flow_ml_s = flow;
  /** Real scale weight will arrive via a dedicated BLE path once integrated. */
  s_live.weight_g = 0.f;
  s_live.shot_time_s = t_s;
  /** Shot-sample stream is continuous; only surface numbers while pulling a shot. */
  s_live.has_live_data = de1_shot_time_should_show_unlocked();

  snprintf(s_live.detail_line, sizeof(s_live.detail_line), "%.*s",
           (int)sizeof(s_remote_name_snapshot), s_remote_name_snapshot);
  return true;
}

static void scan_resume(void);

static bool name_matches_de1_pattern(const uint8_t *nm, uint8_t len) {
  if (nm == NULL || len == 0) {
    return false;
  }

  /** DE1 advertiser name heuristic (stack buffer, no heap). */
  char lower[41];
  size_t copy = len;
  if (copy >= sizeof(lower)) {
    copy = sizeof(lower) - 1;
  }

  size_t span = copy;
  for (size_t i = 0; i < copy && i < sizeof(lower) - 1; ++i) {
    unsigned char ch = nm[i];
    if (ch == '\0') {
      span = i;
      break;
    }
    lower[i] = (char)tolower((int)ch);
  }
  lower[span] = '\0';

  if (!strcmp(lower, "nrf5x")) {
    return true;
  }
  if (strstr(lower, "de1") || strstr(lower, "decent") || strstr(lower, "de1pro") ||
      strstr(lower, "de1+") || strstr(lower, "de1 ")) {
    return true;
  }
  return false;
}

static bool adv_announces_de1_service(const struct ble_hs_adv_fields *fields) {
  for (unsigned i = 0; i < fields->num_uuids128; ++i) {
    if (ble_uuid_cmp((const ble_uuid_t *)&s_uuid_svc, &fields->uuids128[i].u) == 0) {
      return true;
    }
  }
  for (unsigned i = 0; i < fields->sol_num_uuids128; ++i) {
    if (ble_uuid_cmp((const ble_uuid_t *)&s_uuid_svc, &fields->sol_uuids128[i].u) == 0) {
      return true;
    }
  }
  return false;
}

static bool is_de1_candidate(struct ble_gap_disc_desc const *disc, struct ble_hs_adv_fields *fields,
                             bool *matched_uuid_out, bool *matched_name_out) {
  if (matched_uuid_out != NULL) {
    *matched_uuid_out = false;
  }
  if (matched_name_out != NULL) {
    *matched_name_out = false;
  }

  if (disc == NULL || fields == NULL) {
    return false;
  }
  if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
      disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP &&
      disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {
    return false;
  }

  bool uuid_hit = adv_announces_de1_service(fields);
  bool name_hit = name_matches_de1_pattern(fields->name, fields->name_len);
  if (matched_uuid_out != NULL) {
    *matched_uuid_out = uuid_hit;
  }
  if (matched_name_out != NULL) {
    *matched_name_out = name_hit;
  }
  return uuid_hit || name_hit;
}

static bool connect_to_peer(const ble_addr_t *peer_addr) {
  int rc;

  if (peer_addr == NULL) {
    return false;
  }

  uint8_t own_addr_type = 0;
  rc = ble_hs_id_infer_auto(0, &own_addr_type);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_hs_id_infer_auto failed rc=%d", rc);
    return false;
  }

#if !MYNEWT_VAL(BLE_HOST_ALLOW_CONNECT_WITH_SCAN)
  rc = ble_gap_disc_cancel();
  if (rc != 0) {
    ESP_LOGW(TAG, "ble_gap_disc_cancel failed rc=%d (cannot initiate connection yet)", rc);
    return false;
  }
#endif

  ESP_LOGI(TAG, "BLE: connecting to peer=\"%s\" addr=%s",
           s_remote_name_snapshot[0] != '\0' ? s_remote_name_snapshot : "(no name)",
           addr_str(peer_addr->val));

  rc = ble_gap_connect(own_addr_type, peer_addr, 60000, &k_de1_conn_params, gap_event, NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gap_connect failed rc=%d", rc);
    scan_resume();
    return false;
  }

  return true;
}

static void scan_resume(void) {
  if (!s_nimble_ready) {
    return;
  }
  if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE || s_discovery_pending || s_gatt_ready ||
      s_connect_pending) {
    return;
  }

  locked_heading_detail_scan(true, "Bluetooth: scanning", "Scanning for DE1…");
  ESP_LOGI(TAG, "BLE: searching for DE1");

  uint8_t own_addr_type = 0;
  int rc = ble_hs_id_infer_auto(0, &own_addr_type);
  if (rc != 0) {
    ESP_LOGE(TAG, "scan_resume infer addr rc=%d", rc);
    return;
  }

  struct ble_gap_disc_params p = {.filter_duplicates = 0};
  /** Active scan so SCAN_RSP includes the device name. */
  p.passive = 0;

  rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &p, gap_event, NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gap_disc failed rc=%d", rc);
  }
}

static int gap_event(struct ble_gap_event *event, void *arg) {
  (void)arg;
  struct ble_gap_conn_desc conn_desc;

  switch (event->type) {
  case BLE_GAP_EVENT_DISC: {
    ++s_gap_adv_reports_total;
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) != 0) {
      /** Common with random peripherals — not an error for us; avoid WARN spam on console. */
      ESP_LOGD(TAG, "ADV parse failed (addr=%s len=%u)", addr_str(event->disc.addr.val),
               (unsigned)event->disc.length_data);
      break;
    }

    if (!is_de1_candidate(&event->disc, &fields, NULL, NULL)) {
      break;
    }

    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE || s_discovery_pending || s_connect_pending) {
      break;
    }

    ++s_gap_adv_de1_matches;

    snprintf(s_remote_name_snapshot, sizeof(s_remote_name_snapshot), "(no name)");
    if (fields.name_len > 0 && fields.name != NULL) {
      size_t n = fields.name_len;
      if (n >= sizeof(s_remote_name_snapshot)) {
        n = sizeof(s_remote_name_snapshot) - 1;
      }
      memcpy(s_remote_name_snapshot, fields.name, n);
      /** Trim embedded NULs / trailing whitespace. */
      while (n > 0 && (s_remote_name_snapshot[n - 1] == '\0' || isspace((unsigned char)s_remote_name_snapshot[n - 1]))) {
        n--;
      }
      s_remote_name_snapshot[n] = '\0';
    }

    if (xSemaphoreTake(s_status_mtx, pdMS_TO_TICKS(250)) != pdTRUE) {
      break;
    }
    memcpy(s_live.peer_addr_be, event->disc.addr.val, sizeof(s_live.peer_addr_be));
    xSemaphoreGive(s_status_mtx);

    char heading_found[sizeof(s_live.ble_heading)];
    snprintf(heading_found, sizeof heading_found,
             "Bluetooth: found %.32s",
             (s_remote_name_snapshot[0] != '\0') ? s_remote_name_snapshot : "?");

    ESP_LOGI(TAG, "BLE: found peer=\"%s\" addr=%s",
             s_remote_name_snapshot[0] != '\0' ? s_remote_name_snapshot : "(no name)",
             addr_str(event->disc.addr.val));

    s_pending_peer_addr = event->disc.addr;
    s_connect_pending = true;
    locked_heading_detail_scan(false, heading_found, "Connecting…");
    return 0;
  }

  case BLE_GAP_EVENT_CONNECT: {
    s_connect_pending = false;

    if (event->connect.status != 0) {
      ESP_LOGW(TAG, "BLE: connect failed status=%d (resuming search)",
               event->connect.status);
      peer_delete(event->connect.conn_handle);
      s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
      s_gatt_ready = false;
      s_discovery_pending = false;
      status_reset_disconnected(true);
      scan_resume();
      return 0;
    }

    s_conn_handle = event->connect.conn_handle;

    struct ble_gap_conn_desc connected;
    memset(&connected, 0, sizeof(connected));
    const char *peer_ota_str = "?";
    if (ble_gap_conn_find(s_conn_handle, &connected) == 0) {
      peer_ota_str = addr_str(connected.peer_ota_addr.val);
    }
    ESP_LOGI(TAG, "BLE: link up conn=%u peer=%s", (unsigned)s_conn_handle, peer_ota_str);

    char detail_connected[sizeof(s_live.detail_line)];
    snprintf(detail_connected, sizeof detail_connected, "%s (@%s)",
             s_remote_name_snapshot, addr_str(s_live.peer_addr_be));

    if (peer_add(s_conn_handle) != 0) {
      ESP_LOGE(TAG, "peer_add failed");
      ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
      break;
    }

    s_discovery_pending = true;

    int rc_disc =
        peer_disc_svc_by_uuid(s_conn_handle, (const ble_uuid_t *)&s_uuid_svc, on_disc_complete, NULL);
    if (rc_disc != 0) {
      ESP_LOGE(TAG, "peer_disc_svc_by_uuid failed rc=%d", rc_disc);
      s_discovery_pending = false;
      ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }

    locked_heading_detail_scan(false, "Bluetooth: discovering…", detail_connected);
    return 0;
  }

  case BLE_GAP_EVENT_NOTIFY_RX: {
    if (event->notify_rx.conn_handle != s_conn_handle || !s_gatt_ready) {
      return 0;
    }
    uint16_t h = event->notify_rx.attr_handle;
    if (h == s_state_val_handle && s_state_val_handle != 0) {
      uint8_t st[4];
      uint16_t raw_len = OS_MBUF_PKTLEN(event->notify_rx.om);
      uint16_t n = raw_len > sizeof(st) ? sizeof(st) : raw_len;
      if (n >= 2 && os_mbuf_copydata(event->notify_rx.om, 0, n, st) == 0) {
        if (xSemaphoreTake(s_status_mtx, portMAX_DELAY) == pdTRUE) {
          s_live.connected = true;
          de1_apply_state_bytes_unlocked(st[0], st[1]);
          xSemaphoreGive(s_status_mtx);
        }
      }
      return 0;
    }
    if (h != s_shot_val_handle) {
      return 0;
    }
    /** Copy notification payload safely for parsing. */
    uint8_t pkt[64];
    uint16_t raw_len = OS_MBUF_PKTLEN(event->notify_rx.om);
    uint16_t pktlen = raw_len;
    if (pktlen > sizeof(pkt)) {
      ESP_LOGW(TAG, "shot notify truncated mbuf_len=%u cap=%u", (unsigned)raw_len,
             (unsigned)sizeof(pkt));
      pktlen = sizeof(pkt);
    }
    int mbuf_rc = os_mbuf_copydata(event->notify_rx.om, 0, pktlen, pkt);
    if (mbuf_rc != 0) {
      ESP_LOGW(TAG, "shot notify mbuf copy rc=%d len=%u", mbuf_rc, (unsigned)pktlen);
      return 0;
    }

    bool parsed_ok = false;
    if (xSemaphoreTake(s_status_mtx, portMAX_DELAY) == pdTRUE) {
      s_live.connected = true;
      parsed_ok = telemetry_apply_unlocked(pkt, pktlen);
      xSemaphoreGive(s_status_mtx);
    }

    if (!parsed_ok) {
      ESP_LOGW(TAG, "shot sample parse failed len=%u (need >=14 bytes)", (unsigned)pktlen);
    }

    return 0;
  }

  case BLE_GAP_EVENT_DISCONNECT: {
    int reason = event->disconnect.reason;
    if (reason >= BLE_HS_ERR_HCI_BASE && reason < BLE_HS_ERR_HCI_BASE + 0x100) {
      ESP_LOGI(TAG, "BLE: disconnected hci=0x%02x (scan resumes)", reason - BLE_HS_ERR_HCI_BASE);
    } else {
      ESP_LOGI(TAG, "BLE: disconnected status=%d (scan resumes)", reason);
    }

    peer_delete(event->disconnect.conn.conn_handle);

    if (event->disconnect.conn.conn_handle == s_conn_handle) {
      s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    }

    s_connect_pending = false;
    memset(s_remote_name_snapshot, 0, sizeof(s_remote_name_snapshot));
    status_reset_disconnected(true);
    s_gatt_ready = false;
    s_discovery_pending = false;
    s_shot_val_handle = 0;
    s_state_val_handle = 0;
    s_requested_state_val_handle = 0;
    s_last_state_read_us = 0;
    s_scale_connected = false;
    s_scale_has_weight = false;
    s_scale_weight_g = 0.f;
    scan_resume();
    return 0;
  }

  case BLE_GAP_EVENT_DISC_COMPLETE:
    ESP_LOGD(TAG, "GAP scan procedure complete status=%d", event->disc_complete.reason);
    return 0;

  case BLE_GAP_EVENT_TERM_FAILURE:
    ESP_LOGW(TAG, "GAP terminate failed conn=%u status=%d",
             (unsigned)event->term_failure.conn_handle,
             event->term_failure.status);
    return 0;

  case BLE_GAP_EVENT_MTU:
    if (event->mtu.conn_handle == s_conn_handle) {
      ESP_LOGD(TAG, "GAP ATT MTU update conn=%u mtu=%u ch=0x%04x",
               (unsigned)event->mtu.conn_handle,
               (unsigned)event->mtu.value,
               (unsigned)event->mtu.channel_id);
    }
    return 0;

  case BLE_GAP_EVENT_REPEAT_PAIRING: {
    int rc_rp = ble_gap_conn_find(event->repeat_pairing.conn_handle, &conn_desc);
    if (rc_rp == 0) {
      ble_store_util_delete_peer(&conn_desc.peer_id_addr);
    }
    return BLE_GAP_REPEAT_PAIRING_RETRY;
  }

  default:
    return 0;
  }

  return 0;
}

static int on_shot_cccd_written(uint16_t conn_handle,
                                const struct ble_gatt_error *error,
                                struct ble_gatt_attr *attr,
                                void *arg) {
  (void)attr;
  (void)arg;

  if (error->status != 0) {
    ESP_LOGE(TAG, "ShotSample CCCD write failed status=%d", error->status);
    ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    return 0;
  }

  s_gatt_ready = true;

  char peer_log[sizeof(s_remote_name_snapshot)];

  if (xSemaphoreTake(s_status_mtx, pdMS_TO_TICKS(500)) != pdTRUE) {
    ESP_LOGW(TAG, "Failed to latch streaming banner");
    return 0;
  }

  strncpy(s_live.ble_heading, "Bluetooth: DE1 streaming", sizeof(s_live.ble_heading) - 1);
  s_live.ble_heading[sizeof(s_live.ble_heading) - 1] = '\0';
  s_live.connected = true;
  /** Wait for notifications for live gauges. */
  s_live.has_live_data = false;
  s_live.scanning = false;

  strncpy(peer_log, s_remote_name_snapshot, sizeof peer_log);
  peer_log[sizeof peer_log - 1] = '\0';

  xSemaphoreGive(s_status_mtx);

  read_de1_state_now(conn_handle);

  ESP_LOGI(TAG, "BLE: connected peer=\"%s\"", peer_log[0] != '\0' ? peer_log : "(unknown)");
  return 0;
}

static int on_state_gatt_read(uint16_t conn_handle, const struct ble_gatt_error *error,
                              struct ble_gatt_attr *attr, void *arg) {
  (void)conn_handle;
  (void)arg;

  if (error != NULL && error->status != 0) {
    ESP_LOGD(TAG, "StateInfo read failed status=%d", error->status);
    return 0;
  }
  if (attr == NULL || attr->om == NULL) {
    return 0;
  }

  uint8_t st[4];
  uint16_t raw_len = OS_MBUF_PKTLEN(attr->om);
  uint16_t n = raw_len > sizeof(st) ? sizeof(st) : raw_len;
  if (n < 2) {
    return 0;
  }
  if (os_mbuf_copydata(attr->om, 0, n, st) != 0) {
    return 0;
  }

  if (xSemaphoreTake(s_status_mtx, portMAX_DELAY) == pdTRUE) {
    s_live.connected = true;
    de1_apply_state_bytes_unlocked(st[0], st[1]);
    xSemaphoreGive(s_status_mtx);
  }
  return 0;
}

static void read_de1_state_now(uint16_t conn_handle) {
  if (s_state_val_handle == 0 || conn_handle == BLE_HS_CONN_HANDLE_NONE) {
    return;
  }
  int rc = ble_gattc_read(conn_handle, s_state_val_handle, on_state_gatt_read, NULL);
  if (rc != 0) {
    ESP_LOGD(TAG, "ble_gattc_read(StateInfo) rc=%d", rc);
  } else {
    s_last_state_read_us = esp_timer_get_time();
  }
}

static int on_state_cccd_written(uint16_t conn_handle,
                                 const struct ble_gatt_error *error,
                                 struct ble_gatt_attr *attr,
                                 void *arg) {
  (void)attr;
  (void)arg;

  if (error->status != 0) {
    ESP_LOGE(TAG, "StateInfo CCCD write failed status=%d", error->status);
    ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    return 0;
  }

  const struct peer *peer = peer_find(conn_handle);
  if (peer == NULL) {
    ESP_LOGE(TAG, "StateInfo CCCD ok but peer missing");
    return 0;
  }

  subscribe_to_shot(peer);
  return 0;
}

static void subscribe_to_state(const struct peer *peer) {
  const struct peer_chr *chr =
      peer_chr_find_uuid(peer, (const ble_uuid_t *)&s_uuid_svc, (const ble_uuid_t *)&s_uuid_state);
  if (chr == NULL) {
    ESP_LOGW(TAG, "StateInfo missing; shot stream only");
    subscribe_to_shot(peer);
    return;
  }

  s_state_val_handle = chr->chr.val_handle;

  const struct peer_dsc *ccd = peer_dsc_find_uuid(peer,
                                                   (const ble_uuid_t *)&s_uuid_svc,
                                                   (const ble_uuid_t *)&s_uuid_state,
                                                   BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
  if (ccd == NULL) {
    ESP_LOGW(TAG, "StateInfo CCCD missing; polling only");
    read_de1_state_now(peer->conn_handle);
    subscribe_to_shot(peer);
    return;
  }

  uint8_t notify_on[2] = {1, 0};
  int rc = ble_gattc_write_flat(peer->conn_handle, ccd->dsc.handle,
                                notify_on, sizeof notify_on,
                                on_state_cccd_written,
                                NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gattc_write_flat(StateInfo cccd) failed rc=%d", rc);
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
  }
}

static void subscribe_to_shot(const struct peer *peer) {
  const struct peer_chr *chr =
      peer_chr_find_uuid(peer, (const ble_uuid_t *)&s_uuid_svc, (const ble_uuid_t *)&s_uuid_shot);
  if (chr == NULL) {
    ESP_LOGE(TAG, "Shot sample characteristic not found");
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    return;
  }

  const struct peer_dsc *ccd = peer_dsc_find_uuid(peer,
                                                   (const ble_uuid_t *)&s_uuid_svc,
                                                   (const ble_uuid_t *)&s_uuid_shot,
                                                   BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
  if (ccd == NULL) {
    ESP_LOGE(TAG, "CCCD for shot characteristic missing");
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    return;
  }

  s_shot_val_handle = chr->chr.val_handle;

  uint8_t notify_on[2] = {1, 0};
  int rc = ble_gattc_write_flat(peer->conn_handle, ccd->dsc.handle,
                                notify_on, sizeof notify_on,
                                on_shot_cccd_written,
                                NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gattc_write_flat(cccd) failed rc=%d", rc);
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
  }
}

static void on_disc_complete(const struct peer *peer, int status, void *arg) {
  (void)arg;

  s_discovery_pending = false;

  if (status != 0) {
    ESP_LOGE(TAG, "GATT discovery failed status=%d", status);
    if (peer != NULL) {
      ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    return;
  }

  ESP_LOGD(TAG, "GATT discovery complete conn=%u", peer != NULL ? (unsigned)peer->conn_handle : 0u);

  locked_heading_detail_scan(false, "Bluetooth: DE1 subscribed…", NULL);

  const struct peer_chr *requested_state_chr = peer_chr_find_uuid(
      peer, (const ble_uuid_t *)&s_uuid_svc, (const ble_uuid_t *)&s_uuid_requested_state);
  if (requested_state_chr == NULL) {
    ESP_LOGW(TAG, "RequestedState characteristic not found (0xa002)");
    s_requested_state_val_handle = 0;
  } else {
    s_requested_state_val_handle = requested_state_chr->chr.val_handle;
  }

  subscribe_to_state(peer);
}

static void on_reset(int reason) {
  ESP_LOGE(TAG, "NimBLE reset; reason=%d", reason);
}

static void on_sync(void) {
  s_nimble_ready = true;

  if (ble_uuid_from_str(&s_uuid_svc, k_svc_str) != 0 ||
      ble_uuid_from_str(&s_uuid_shot, k_shot_chr_str) != 0 ||
      ble_uuid_from_str(&s_uuid_state, k_state_chr_str) != 0 ||
      ble_uuid_from_str(&s_uuid_requested_state, k_requested_state_chr_str) != 0) {
    ESP_LOGE(TAG, "Invalid DE1 BLE UUID constants");
    return;
  }

  int rc = ble_hs_util_ensure_addr(0);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_hs_util_ensure_addr failed rc=%d", rc);
  }

  ESP_LOGD(TAG,
           "NimBLE synced; DE1 svc %s shot %s (GAP \"%s\")",
           k_svc_str, k_shot_chr_str, deui_ble_gap_name());

#ifdef CONFIG_BT_NIMBLE_GAP_SERVICE
  (void)ble_svc_gap_device_name_set(deui_ble_gap_name());
#endif

  status_reset_disconnected(true);
  scan_resume();
}

static void host_task(void *param) {
  (void)param;
  nimble_port_run();
  nimble_port_freertos_deinit();
}

esp_err_t deui_ble_init(void) {
  memset(&s_live, 0, sizeof(s_live));
  memset(s_remote_name_snapshot, 0, sizeof(s_remote_name_snapshot));

  s_status_mtx = xSemaphoreCreateMutex();
  if (s_status_mtx == NULL) {
    return ESP_ERR_NO_MEM;
  }

  status_reset_disconnected(false);

  esp_err_t er = nimble_port_init();
  if (er != ESP_OK) {
    ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(er));
    return er;
  }

  ble_hs_cfg.reset_cb = on_reset;
  ble_hs_cfg.sync_cb = on_sync;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  int rp = peer_init(3, 32, 32, 48);
  if (rp != 0) {
    ESP_LOGE(TAG, "peer_init failed rc=%d", rp);
    return ESP_FAIL;
  }

  ble_store_config_init();

  nimble_port_freertos_init(host_task);

  ESP_LOGD(TAG, "NimBLE host started (GAP \"%s\")", deui_ble_gap_name());
  return ESP_OK;
}

void deui_ble_tick(void) {
  if (!s_nimble_ready) {
    return;
  }

  if (s_connect_pending && s_conn_handle == BLE_HS_CONN_HANDLE_NONE && !s_discovery_pending) {
    s_connect_pending = false;
    if (!connect_to_peer(&s_pending_peer_addr)) {
      status_reset_disconnected(true);
      scan_resume();
    }
  }

  int64_t now = esp_timer_get_time();

  if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE && s_state_val_handle != 0 && !s_discovery_pending) {
    if (now - s_last_state_read_us >= 250000) {
      read_de1_state_now(s_conn_handle);
    }
  }

  if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE || s_discovery_pending || s_gatt_ready ||
      s_connect_pending) {
    return;
  }

  static int64_t s_next_hb_us;
  if (s_next_hb_us == 0) {
    s_next_hb_us = now + 5000000; /* First heartbeat ~5 s after NimBLE scanning starts */
    return;
  }
  if (now < s_next_hb_us) {
    return;
  }
  s_next_hb_us = now + 12000000;

  ESP_LOGD(TAG,
           "BLE: scan heartbeat adv_total=%lu de1_matches=%lu",
           (unsigned long)s_gap_adv_reports_total, (unsigned long)s_gap_adv_de1_matches);
}

void deui_ble_get_status(deui_ble_status_t *status) {
  if (status == NULL) {
    return;
  }
  if (xSemaphoreTake(s_status_mtx, pdMS_TO_TICKS(120)) != pdTRUE) {
    memset(status, 0, sizeof(*status));
    return;
  }

  memcpy(status, &s_live, sizeof(*status));

  /** Ensure UI observes connection latch while GATT is active. */
  if (s_gatt_ready) {
    status->connected = true;
    if (strlen(status->detail_line) == 0 &&
        memcmp(status->peer_addr_be, k_zero_mac, sizeof(status->peer_addr_be)) != 0) {
      snprintf(status->detail_line, sizeof(status->detail_line),
               "%.*s (@%02x:%02x:%02x:%02x:%02x:%02x)",
               (int)sizeof(s_remote_name_snapshot), s_remote_name_snapshot,
               status->peer_addr_be[5], status->peer_addr_be[4], status->peer_addr_be[3],
               status->peer_addr_be[2], status->peer_addr_be[1], status->peer_addr_be[0]);
    }
  }

  if (!status->connected) {
    if (status->scanning) {
      strncpy(status->machine_state_label, "Searching", sizeof(status->machine_state_label) - 1);
      status->machine_state_label[sizeof(status->machine_state_label) - 1] = '\0';
    } else if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE && !s_gatt_ready) {
      strncpy(status->machine_state_label, "Connecting", sizeof(status->machine_state_label) - 1);
      status->machine_state_label[sizeof(status->machine_state_label) - 1] = '\0';
    }
  } else if (!status->de1_state_valid) {
    strncpy(status->machine_state_label, "Waiting...", sizeof(status->machine_state_label) - 1);
    status->machine_state_label[sizeof(status->machine_state_label) - 1] = '\0';
  }

  bool shot_time =
      status->connected && status->de1_state_valid && (status->de1_major_state == DE1_MAJOR_STATE_ESPRESSO);
  status->show_shot_time = shot_time;
  status->show_scale_weight = shot_time && s_scale_connected;

  if (!shot_time) {
    status->weight_g = 0.f;
    status->shot_time_s = 0.f;
    status->flow_ml_s = 0.f;
    status->pressure_bar = 0.f;
    status->has_live_data = false;
  } else {
    if (status->show_scale_weight) {
      status->weight_g = s_scale_weight_g;
    } else {
      status->weight_g = 0.f;
    }
  }

  /** Host still scanning / connecting counts as non-idle BLE for UI headings. */
  if (strlen(status->ble_heading) == 0) {
    if (status->scanning) {
      strncpy(status->ble_heading, "Bluetooth: scanning", sizeof(status->ble_heading));
    }
  }

  xSemaphoreGive(s_status_mtx);
}

static uint16_t u16_payload_le(const uint8_t *data) {
  return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static int32_t i24_payload_le(const uint8_t *data) {
  int32_t raw = ((int32_t)data[0]) | ((int32_t)data[1] << 8) | ((int32_t)data[2] << 16);
  if ((raw & 0x00800000) != 0) {
    raw |= (int32_t)0xff000000;
  }
  return raw;
}

bool deui_ble_parse_shot_sample(const uint8_t *payload, size_t len, de1_shot_sample_t *out) {
  if (payload == NULL || out == NULL || len < 14) {
    return false;
  }

  /*
   * DE1 shot sample parsing follows existing DEUI conversion scales:
   * pressure/flow fixed-point values are divided by 4096 and temperatures by 256/65536.
   */
  out->sample_time = u16_payload_le(payload + 0);
  out->group_pressure = (float)i24_payload_le(payload + 2) / 4096.0f;
  out->group_flow = (float)i24_payload_le(payload + 5) / 4096.0f;
  out->mix_temperature = (float)u16_payload_le(payload + 8) / 256.0f;
  out->head_temperature = (float)u16_payload_le(payload + 10) / 65536.0f;
  return true;
}

static const char *const s_gap_name = "DEUI-ST77916";

const char *deui_ble_gap_name(void) {
  return s_gap_name;
}

void deui_ble_set_scale_weight(float weight_g, bool has_weight, bool connected) {
  if (xSemaphoreTake(s_status_mtx, pdMS_TO_TICKS(60)) != pdTRUE) {
    return;
  }
  s_scale_connected = connected;
  s_scale_has_weight = has_weight;
  s_scale_weight_g = weight_g;
  xSemaphoreGive(s_status_mtx);
}

esp_err_t deui_ble_request_idle_stop(void) {
  if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || !s_gatt_ready) {
    return ESP_ERR_INVALID_STATE;
  }
  if (s_requested_state_val_handle == 0) {
    return ESP_ERR_NOT_FOUND;
  }

  uint8_t idle_state = 0x02u;
  int rc = ble_gattc_write_flat(s_conn_handle, s_requested_state_val_handle, &idle_state,
                                sizeof(idle_state), NULL, NULL);
  if (rc != 0) {
    ESP_LOGW(TAG, "RequestedState write failed rc=%d", rc);
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "RequestedState=Idle stop sent");
  return ESP_OK;
}

