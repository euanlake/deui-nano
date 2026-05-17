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

  /** Peer / status line (e.g. DE1 name); shown when useful. */
  char ble_footer[160];

  /**
   * Center label when connected and idle (Waiting..., Idle, Ready, …).
   * Ignored while disconnected (UI forces "Searching") and while extracting (metrics view).
   */
  char machine_state_center[24];

  /**
   * Legacy field — layout uses `de1_major_state` (0x02 = idle UI, 0x04 = brewing UI).
   */
  bool show_shot_time;
  bool show_scale_weight;

  /** From DE1 StateInfo; 0x02 = idle UI, 0x04 = brewing UI (mutually exclusive). */
  bool de1_state_valid;
  uint8_t de1_major_state;
} deui_ui_status_t;

esp_err_t deui_ui_init(lv_disp_t *display);
void deui_ui_update_metrics(float weight_g, float shot_time_s, float flow_ml_s, float pressure_bar,
                            bool show_shot_metrics);
void deui_ui_update_status(const deui_ui_status_t *status);
void deui_ui_indicate_ring_step(int delta);
void deui_ui_tick(void);
