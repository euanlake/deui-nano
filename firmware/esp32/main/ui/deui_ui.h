#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "lvgl.h"

#include "board_power.h"

typedef struct {
  bool ble_connected;
  bool wifi_connected;
  bool scale_connected;
  bool scale_scanning;
  lm_ctrl_power_info_t power;

  /** Peer / status line while disconnected (e.g. Wi-Fi setup); hidden when BLE is connected. */
  char ble_footer[160];
  char wifi_footer[160];

  /**
   * Center label on the idle layout (Waiting..., Idle, Ready, …).
   * Ignored while disconnected ("Searching") and during live extraction (brewing layout).
   */
  char machine_state_center[24];

  /**
   * BLE layer flags for shot timer / scale weight in telemetry; UI mode is driven by `de1_major_state`.
   */
  bool show_shot_time;
  bool show_scale_weight;

  /** From DE1 StateInfo; when connected and major ≠ Espresso (0x04), idle layout is shown regardless of validity. */
  bool de1_state_valid;
  uint8_t de1_major_state;
  uint8_t de1_minor_state;

  /**
   * When connected and on idle layout: show the metrics grid with saved post-shot peaks.
   * Ignored while searching or on the live brewing layout.
   */
  bool show_saved_shot_metrics;
} deui_ui_status_t;

esp_err_t deui_ui_init(lv_disp_t *display);
void deui_ui_update_metrics(float weight_g, float shot_time_s, float pressure_bar, float flow_ml_s,
                            bool show_shot_metrics, bool metrics_live);
void deui_ui_update_status(const deui_ui_status_t *status);
void deui_ui_indicate_ring_step(int delta);
void deui_ui_tick(void);
