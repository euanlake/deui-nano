#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * Per-shot rolling maxima + post-shot display (mirrors DEUI app `RecentEspressoMax*` /
 * live-vs-peak metrics — see `deui-app/docs/espresso-max-flow-dip-detection.md`).
 */
typedef struct {
  float weight_g;
  float shot_time_s;
  float pressure_bar;
  float flow_ml_s;
} deui_shot_metrics_values_t;

/** Zero tracking and saved snapshot (new espresso session). */
void deui_shot_metrics_on_espresso_enter(void);

/**
 * Call each UI frame with live telemetry and DE1 state.
 * `shot_time_s` is host shot timer (pre-infusion onward), not DE1 sample_time.
 */
void deui_shot_metrics_update(bool in_espresso, uint8_t minor_state, uint8_t prev_minor_state,
                              uint8_t prev_major_state, float live_weight_g, float live_shot_time_s,
                              float live_pressure_bar, float live_flow_ml_s);

/** Snapshot peaks when leaving espresso major (end of pull). */
void deui_shot_metrics_on_espresso_exit(float final_shot_time_s);

/**
 * Resolve metrics grid values and presentation.
 * `out_show`: render the 2×2 grid (live pull or saved post-shot).
 * `out_live`: strong text colour + live captions; false → subdued + max captions.
 */
void deui_shot_metrics_resolve(bool in_espresso, uint8_t minor_state, float live_weight_g,
                               float live_shot_time_s, float live_pressure_bar, float live_flow_ml_s,
                               deui_shot_metrics_values_t *out_values, bool *out_show, bool *out_live);
