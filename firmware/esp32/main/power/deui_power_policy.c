#include "deui_power_policy.h"

void deui_power_policy_init(deui_power_policy_t *policy, uint32_t idle_timeout_ms) {
  if (policy == NULL) {
    return;
  }
  policy->state = DEUI_POWER_POLICY_AWAKE;
  policy->last_activity_us = esp_timer_get_time();
  policy->idle_timeout_ms = idle_timeout_ms;
}

void deui_power_policy_note_activity(deui_power_policy_t *policy, int64_t now_us) {
  if (policy == NULL) {
    return;
  }
  policy->last_activity_us = now_us;
}

bool deui_power_line_power(const lm_ctrl_power_info_t *power) {
  if (power == NULL) {
    return false;
  }
  return power->usb_connected || power->charging;
}

deui_power_policy_state_t deui_power_policy_step(deui_power_policy_t *policy,
                                                 const lm_ctrl_power_info_t *power,
                                                 int64_t now_us) {
  if (policy == NULL) {
    return DEUI_POWER_POLICY_AWAKE;
  }

  const int64_t idle_timeout_us = (int64_t)policy->idle_timeout_ms * 1000LL;
  const bool idle_timed_out = (now_us - policy->last_activity_us) >= idle_timeout_us;
  const bool line_power = deui_power_line_power(power);

  if (!idle_timed_out) {
    policy->state = DEUI_POWER_POLICY_AWAKE;
    return policy->state;
  }

  policy->state = line_power ? DEUI_POWER_POLICY_AC_DIMMED : DEUI_POWER_POLICY_BAT_SLEEP;
  return policy->state;
}
