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

esp_err_t deui_weight_stop_init(void);

void deui_weight_stop_tick(bool de1_connected, uint8_t major_state, uint8_t minor_state,
                           bool scale_connected, bool has_weight, float weight_g);

void deui_weight_stop_get_status(deui_weight_stop_status_t *status);
