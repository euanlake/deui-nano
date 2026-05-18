#include "deui_shot_metrics.h"

#include "deui_ble_client.h"
#include "esp_timer.h"

/** Minor states that show live shot telemetry (DEUI app “busy” minors). */
enum {
  k_minor_heat_water_heater = 0x02,
  k_minor_preinfuse = 0x04,
  k_minor_pour = 0x05,
  k_minor_flush = 0x06,
};

/** Post-pour weight peak window (matches DEUI `POST_POUR_WEIGHT` ~2 s). */
enum {
  k_post_pour_weight_us = 2000000,
};

/** Dip-gated max flow — keep in sync with `deui-app` device store constants. */
enum {
  k_flow_dip_threshold_ratio_num = 70,
  k_flow_dip_threshold_ratio_den = 100,
  k_flow_dip_fallback_us = 4000000,
};
static const float k_flow_min_peak_ml_s = 0.5f;

static bool minor_is_busy_extraction(uint8_t minor) {
  return minor == k_minor_heat_water_heater || minor == k_minor_preinfuse || minor == k_minor_pour ||
         minor == k_minor_flush;
}

static deui_shot_metrics_values_t s_saved = {0};
static bool s_has_saved = false;

static float s_max_weight_g = 0.f;
static float s_max_pressure_bar = 0.f;
static float s_max_flow_ml_s = 0.f;

static int64_t s_pour_start_us = 0;
static float s_pour_flow_peak = 0.f;
static bool s_pour_flow_max_tracking_enabled = false;
static int64_t s_post_pour_until_us = 0;

static void reset_pour_dip_state(void) {
  s_pour_start_us = 0;
  s_pour_flow_peak = 0.f;
  s_pour_flow_max_tracking_enabled = false;
  s_post_pour_until_us = 0;
}

static void reset_session_trackers(void) {
  s_max_weight_g = 0.f;
  s_max_pressure_bar = 0.f;
  s_max_flow_ml_s = 0.f;
  reset_pour_dip_state();
}

void deui_shot_metrics_on_espresso_enter(void) {
  reset_session_trackers();
  s_has_saved = false;
  s_saved = (deui_shot_metrics_values_t){0};
}

void deui_shot_metrics_on_espresso_exit(float final_shot_time_s) {
  s_saved.weight_g = s_max_weight_g;
  s_saved.pressure_bar = s_max_pressure_bar;
  s_saved.flow_ml_s = s_max_flow_ml_s;
  s_saved.shot_time_s = final_shot_time_s;
  s_has_saved = true;
  reset_session_trackers();
}

static bool just_entered_pour(uint8_t minor, uint8_t prev_minor, uint8_t prev_major) {
  return minor == k_minor_pour &&
         (prev_minor != k_minor_pour || prev_major != DE1_MAJOR_STATE_ESPRESSO);
}

static void on_entered_pour(void) {
  s_pour_start_us = esp_timer_get_time();
  s_pour_flow_peak = 0.f;
  s_pour_flow_max_tracking_enabled = false;
}

static void update_max_flow_dip_gated(float flow_ml_s) {
  if (flow_ml_s > s_pour_flow_peak) {
    s_pour_flow_peak = flow_ml_s;
  }

  if (!s_pour_flow_max_tracking_enabled) {
    const bool dip_detected = s_pour_flow_peak > k_flow_min_peak_ml_s &&
                              flow_ml_s < (s_pour_flow_peak * (float)k_flow_dip_threshold_ratio_num /
                                           (float)k_flow_dip_threshold_ratio_den);
    const bool fallback =
        s_pour_start_us > 0 &&
        (esp_timer_get_time() - s_pour_start_us) >= (int64_t)k_flow_dip_fallback_us;

    if (dip_detected) {
      s_max_flow_ml_s = 0.f;
      s_pour_flow_max_tracking_enabled = true;
    } else if (fallback) {
      s_pour_flow_max_tracking_enabled = true;
    }
  }

  if (s_pour_flow_max_tracking_enabled && flow_ml_s > s_max_flow_ml_s) {
    s_max_flow_ml_s = flow_ml_s;
  }
}

static void track_weight(float weight_g, uint8_t minor) {
  const int64_t now_us = esp_timer_get_time();
  if (minor == k_minor_pour) {
    if (weight_g > s_max_weight_g) {
      s_max_weight_g = weight_g;
    }
    return;
  }
  if (s_post_pour_until_us > 0 && now_us <= s_post_pour_until_us) {
    if (weight_g > s_max_weight_g) {
      s_max_weight_g = weight_g;
    }
  }
}

void deui_shot_metrics_update(bool in_espresso, uint8_t minor_state, uint8_t prev_minor_state,
                              uint8_t prev_major_state, float live_weight_g, float live_shot_time_s,
                              float live_pressure_bar, float live_flow_ml_s) {
  if (!in_espresso) {
    return;
  }

  if (just_entered_pour(minor_state, prev_minor_state, prev_major_state)) {
    on_entered_pour();
  }
  if (prev_minor_state == k_minor_pour && minor_state != k_minor_pour) {
    s_post_pour_until_us = esp_timer_get_time() + (int64_t)k_post_pour_weight_us;
  }

  track_weight(live_weight_g, minor_state);

  if (minor_state == k_minor_pour) {
    if (live_pressure_bar > s_max_pressure_bar) {
      s_max_pressure_bar = live_pressure_bar;
    }
    update_max_flow_dip_gated(live_flow_ml_s);
  }

  (void)live_shot_time_s;
}

static void fill_live(deui_shot_metrics_values_t *out, uint8_t minor_state, float live_weight_g,
                      float live_shot_time_s, float live_pressure_bar, float live_flow_ml_s) {
  out->weight_g = live_weight_g;
  out->shot_time_s = live_shot_time_s;
  if (minor_state == k_minor_pour) {
    out->pressure_bar = live_pressure_bar;
    out->flow_ml_s = live_flow_ml_s;
  } else {
    out->pressure_bar = 0.f;
    out->flow_ml_s = 0.f;
  }
}

static void fill_session_peaks(deui_shot_metrics_values_t *out, float live_shot_time_s) {
  out->weight_g = s_max_weight_g;
  out->shot_time_s = live_shot_time_s;
  out->pressure_bar = s_max_pressure_bar;
  out->flow_ml_s = s_max_flow_ml_s;
}

void deui_shot_metrics_resolve(bool in_espresso, uint8_t minor_state, float live_weight_g,
                               float live_shot_time_s, float live_pressure_bar, float live_flow_ml_s,
                               deui_shot_metrics_values_t *out_values, bool *out_show, bool *out_live) {
  if (out_values == NULL || out_show == NULL || out_live == NULL) {
    return;
  }

  *out_values = (deui_shot_metrics_values_t){0};
  *out_show = false;
  *out_live = false;

  if (in_espresso) {
    *out_show = true;
    if (minor_is_busy_extraction(minor_state)) {
      fill_live(out_values, minor_state, live_weight_g, live_shot_time_s, live_pressure_bar, live_flow_ml_s);
      *out_live = true;
    } else {
      fill_session_peaks(out_values, live_shot_time_s);
    }
    return;
  }

  if (s_has_saved) {
    *out_values = s_saved;
    *out_show = true;
  }
}
