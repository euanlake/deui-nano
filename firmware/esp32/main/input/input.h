#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/** Events produced by the physical ring, touch gestures/buttons, and internal runtime wakeups. */
typedef enum {
  LM_CTRL_EVENT_ROTATE = 0,
  LM_CTRL_EVENT_RUNTIME_TICK,
} lm_ctrl_input_event_type_t;

/** Normalized input event consumed by the main controller loop. */
typedef struct {
  lm_ctrl_input_event_type_t type;
  int delta_steps;
} lm_ctrl_input_event_t;

/** Initialize physical input handling and forward events into the supplied queue. */
esp_err_t lm_ctrl_input_init(QueueHandle_t event_queue);
