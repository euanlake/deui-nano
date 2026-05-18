#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board_power.h"
#include "esp_timer.h"

typedef enum {
  DEUI_POWER_POLICY_AWAKE = 0,
  DEUI_POWER_POLICY_SLEEP,
} deui_power_policy_state_t;

typedef struct {
  deui_power_policy_state_t state;
  int64_t last_activity_us;
  uint32_t idle_timeout_ms;
} deui_power_policy_t;

void deui_power_policy_init(deui_power_policy_t *policy, uint32_t idle_timeout_ms);
void deui_power_policy_note_activity(deui_power_policy_t *policy, int64_t now_us);
bool deui_power_line_power(const lm_ctrl_power_info_t *power);
deui_power_policy_state_t deui_power_policy_step(deui_power_policy_t *policy,
                                                 int64_t now_us);
