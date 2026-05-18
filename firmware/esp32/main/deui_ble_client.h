#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define DE1_SERVICE_UUID "0000a000-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_SHOT_SAMPLE "0000a00d-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_STATE_INFO "0000a00e-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_TEMPERATURES "0000a00a-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_REQUESTED_STATE "0000a002-0000-1000-8000-00805f9b34fb"

/** Major states (`docs/de1-bluetooth-protocol.md`) — drive mutually exclusive UI modes. */
#define DE1_MAJOR_STATE_SLEEP 0x00u
#define DE1_MAJOR_STATE_IDLE 0x02u
#define DE1_MAJOR_STATE_ESPRESSO 0x04u

typedef struct {
  uint16_t sample_time;
  float group_pressure;
  float group_flow;
  float mix_temperature;
  float head_temperature;
} de1_shot_sample_t;

typedef struct {
  bool connected;
  bool has_live_data;
  /** True between passive scan sessions (NimBLE still owns radio timing). */
  bool scanning;

  /** DE1 StateInfo (GATT 0xa00e) when available. */
  bool de1_state_valid;
  uint8_t de1_major_state;
  uint8_t de1_minor_state;
  /** User-facing machine state for the main display (Idle, Ready, …). */
  char machine_state_label[24];

  /**
   * When true, shot time may be shown (DE1 is in an espresso pull phase).
   * Bluetooth scale integration can set this later when a scale is linked.
   */
  bool show_shot_time;
  bool show_scale_weight;

  /** Big-endian-ish copy of last peer address for diagnostics (six bytes). */
  uint8_t peer_addr_be[6];

  /** Short heading for BLE / DE1 connection status. */
  char ble_heading[72];

  /** Secondary status / telemetry line. */
  char detail_line[128];

  float pressure_bar;
  float flow_ml_s;
  float weight_g;
  float shot_time_s;
} deui_ble_status_t;

esp_err_t deui_ble_init(void);
esp_err_t deui_ble_suspend(void);
esp_err_t deui_ble_resume(void);
bool deui_ble_is_suspended(void);
void deui_ble_tick(void);
void deui_ble_get_status(deui_ble_status_t *status);
void deui_ble_set_scale_weight(float weight_g, bool has_weight, bool connected);
esp_err_t deui_ble_request_idle_stop(void);
bool deui_ble_parse_shot_sample(const uint8_t *payload, size_t len, de1_shot_sample_t *out);
const char *deui_ble_gap_name(void);
