#include "deui_weight_stop.h"

#include <math.h>
#include <string.h>

#include "deui_ble_client.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "deui_wstop";

enum {
  k_minor_state_pour = 0x05u,
  k_max_weight_points = 96,
};

typedef struct {
  float weight_g;
  int64_t t_us;
} weight_point_t;

/** v1 fixed constants (no user settings yet). */
static const float k_target_weight_g = 40.0f;
static const int64_t k_prediction_warmup_us = 1500000;
static const int64_t k_flow_window_us = 3000000;
static const float k_fallback_flow_g_s = 1.5f;
static const float k_base_stop_delay_s = 0.9f;
static const float k_min_flow_valid_g_s = 0.1f;
static const float k_confidence_threshold = 0.3f;

static bool s_initialized;
static bool s_shot_active;
static bool s_stop_sent;
static int64_t s_shot_start_us;
static weight_point_t s_history[k_max_weight_points];
static size_t s_history_count;

static deui_weight_stop_status_t s_status;

static void reset_shot_state(bool keep_stop_sent);
static void append_weight_point(float weight_g, int64_t now_us);
static void prune_history(int64_t now_us);
static bool compute_flow(float *flow_g_s_out, float *confidence_out);

esp_err_t deui_weight_stop_init(void) {
  memset(&s_status, 0, sizeof(s_status));
  s_status.target_weight_g = k_target_weight_g;
  s_initialized = true;
  return ESP_OK;
}

void deui_weight_stop_get_status(deui_weight_stop_status_t *status) {
  if (status == NULL) {
    return;
  }
  memcpy(status, &s_status, sizeof(*status));
}

void deui_weight_stop_tick(bool de1_connected, uint8_t major_state, uint8_t minor_state,
                           bool scale_connected, bool has_weight, float weight_g) {
  if (!s_initialized) {
    return;
  }

  bool in_pour = de1_connected && major_state == DE1_MAJOR_STATE_ESPRESSO && minor_state == k_minor_state_pour;
  int64_t now_us = esp_timer_get_time();

  if (!in_pour) {
    if (s_shot_active) {
      ESP_LOGI(TAG, "Shot phase ended; clearing predictive state");
    }
    s_shot_active = false;
    reset_shot_state(false);
    s_status.shot_active = false;
    s_status.stop_sent = false;
    return;
  }

  if (!s_shot_active) {
    s_shot_active = true;
    s_shot_start_us = now_us;
    reset_shot_state(false);
    s_status.shot_active = true;
    s_status.stop_sent = false;
    ESP_LOGI(TAG, "Shot phase started; predictive stop armed (target=%.1fg)", k_target_weight_g);
  }

  if (!scale_connected || !has_weight) {
    return;
  }

  append_weight_point(weight_g, now_us);
  prune_history(now_us);

  if (s_stop_sent || now_us - s_shot_start_us < k_prediction_warmup_us) {
    return;
  }

  float flow_g_s = 0.0f;
  float confidence = 0.0f;
  bool have_flow = compute_flow(&flow_g_s, &confidence);
  float effective_flow = k_fallback_flow_g_s;
  if (have_flow && confidence >= k_confidence_threshold && flow_g_s >= k_min_flow_valid_g_s) {
    effective_flow = flow_g_s;
  }

  float weight_remaining = k_target_weight_g - weight_g;
  if (weight_remaining <= 0.0f) {
    weight_remaining = 0.0f;
  }

  float safety_margin = 1.15f + (1.0f - confidence) * 0.20f;
  if (safety_margin < 1.0f) {
    safety_margin = 1.0f;
  }

  float predicted_overrun = effective_flow * k_base_stop_delay_s * safety_margin;
  float stop_threshold = predicted_overrun;
  float target_margin_floor = k_target_weight_g * 0.08f;
  if (stop_threshold < target_margin_floor) {
    stop_threshold = target_margin_floor;
  }

  s_status.last_flow_g_s = effective_flow;
  s_status.last_confidence = confidence;
  s_status.last_predicted_final_weight_g = weight_g + predicted_overrun;

  if (weight_remaining > stop_threshold) {
    return;
  }

  esp_err_t stop_rc = deui_ble_request_idle_stop();
  if (stop_rc == ESP_OK) {
    s_stop_sent = true;
    s_status.stop_sent = true;
    ESP_LOGI(TAG,
             "Predictive stop sent at %.2fg (target %.1fg, flow %.2fg/s, conf %.2f, pred_final %.2fg)",
             weight_g, k_target_weight_g, effective_flow, confidence,
             s_status.last_predicted_final_weight_g);
  } else {
    ESP_LOGW(TAG, "Predictive stop trigger hit but write failed rc=%s", esp_err_to_name(stop_rc));
    /** Prevent repeated spam while DE1 recovers; next shot reset re-arms. */
    s_stop_sent = true;
    s_status.stop_sent = true;
  }
}

static void reset_shot_state(bool keep_stop_sent) {
  s_history_count = 0;
  if (!keep_stop_sent) {
    s_stop_sent = false;
  }
  s_status.last_flow_g_s = 0.0f;
  s_status.last_confidence = 0.0f;
  s_status.last_predicted_final_weight_g = 0.0f;
}

static void append_weight_point(float weight_g, int64_t now_us) {
  if (s_history_count > 0) {
    const weight_point_t *last = &s_history[s_history_count - 1];
    if (now_us <= last->t_us) {
      return;
    }
  }

  if (s_history_count < k_max_weight_points) {
    s_history[s_history_count].weight_g = weight_g;
    s_history[s_history_count].t_us = now_us;
    s_history_count++;
    return;
  }

  memmove(&s_history[0], &s_history[1], sizeof(s_history[0]) * (k_max_weight_points - 1));
  s_history[k_max_weight_points - 1].weight_g = weight_g;
  s_history[k_max_weight_points - 1].t_us = now_us;
}

static void prune_history(int64_t now_us) {
  size_t keep_from = 0;
  int64_t min_keep_us = now_us - k_flow_window_us;
  while (keep_from < s_history_count && s_history[keep_from].t_us < min_keep_us) {
    keep_from++;
  }
  if (keep_from == 0) {
    return;
  }
  if (keep_from >= s_history_count) {
    s_history_count = 0;
    return;
  }
  size_t keep = s_history_count - keep_from;
  memmove(&s_history[0], &s_history[keep_from], sizeof(s_history[0]) * keep);
  s_history_count = keep;
}

static bool compute_flow(float *flow_g_s_out, float *confidence_out) {
  if (flow_g_s_out == NULL || confidence_out == NULL || s_history_count < 3) {
    return false;
  }

  float sum_x = 0.0f;
  float sum_y = 0.0f;
  float sum_xy = 0.0f;
  float sum_x2 = 0.0f;
  float n = (float)s_history_count;
  int64_t t0 = s_history[0].t_us;

  for (size_t i = 0; i < s_history_count; ++i) {
    float x = (float)(s_history[i].t_us - t0) / 1000000.0f;
    float y = s_history[i].weight_g;
    sum_x += x;
    sum_y += y;
    sum_xy += x * y;
    sum_x2 += x * x;
  }

  float denom = n * sum_x2 - sum_x * sum_x;
  if (fabsf(denom) < 0.0001f) {
    *flow_g_s_out = 0.0f;
    *confidence_out = 0.0f;
    return false;
  }

  float slope = (n * sum_xy - sum_x * sum_y) / denom;
  if (slope < 0.0f) {
    slope = 0.0f;
  }
  *flow_g_s_out = slope;

  float intercept = (sum_y - slope * sum_x) / n;
  float mean_y = sum_y / n;
  float ss_res = 0.0f;
  float ss_tot = 0.0f;
  for (size_t i = 0; i < s_history_count; ++i) {
    float x = (float)(s_history[i].t_us - t0) / 1000000.0f;
    float y = s_history[i].weight_g;
    float pred = slope * x + intercept;
    float dy = y - mean_y;
    float err = y - pred;
    ss_res += err * err;
    ss_tot += dy * dy;
  }

  float r2 = 0.0f;
  if (ss_tot > 0.0001f) {
    r2 = 1.0f - (ss_res / ss_tot);
    if (r2 < 0.0f) {
      r2 = 0.0f;
    } else if (r2 > 1.0f) {
      r2 = 1.0f;
    }
  }
  float sample_factor = n / 10.0f;
  if (sample_factor > 1.0f) {
    sample_factor = 1.0f;
  }
  *confidence_out = r2 * sample_factor;
  return true;
}
