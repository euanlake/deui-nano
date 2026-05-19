#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
  bool shot_active;
  bool stop_sent;
  float target_weight_g;
  float last_flow_g_s;
  float last_confidence;
  float last_predicted_final_weight_g;
} deui_weight_stop_status_t;

#define DEUI_WEIGHT_STOP_MAX_G 1000.0f
#define DEUI_WEIGHT_STOP_DEFAULT_G 40.0f

esp_err_t deui_weight_stop_init(void);

/** True when predictive stop is armed (target weight > 0). */
bool deui_weight_stop_is_enabled(void);

/** Enable or disable predictive stop; restores last non-zero target when re-enabled. */
esp_err_t deui_weight_stop_set_enabled(bool enabled);

/** Target in grams; 0 means predictive stop is disabled. */
float deui_weight_stop_get_target_g(void);

/** Persist target (0–1000 g). Values above max are clamped. */
esp_err_t deui_weight_stop_set_target_g(float grams);

/** Adjust saved target by delta grams (e.g. +1 / -1 / +10). Returns new value. */
float deui_weight_stop_adjust_target_g(int delta_g);

void deui_weight_stop_tick(bool de1_connected, uint8_t major_state, uint8_t minor_state,
                           bool scale_connected, bool has_weight, float weight_g);

void deui_weight_stop_get_status(deui_weight_stop_status_t *status);
