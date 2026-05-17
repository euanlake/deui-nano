/**
 * Shared UI symbols for `deui_ui.c` and per-mode screen units (`deui_ui_screen_*.c`).
 * Not a public API — do not include from other firmware modules.
 */
#pragma once

#include "lvgl.h"

extern lv_obj_t *deui_ui_obj_machine_state;
extern lv_obj_t *deui_ui_obj_metrics_card;

void deui_ui_label_set_static_if_changed(lv_obj_t *label, const char *txt);
