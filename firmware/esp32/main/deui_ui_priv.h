/**
 * Shared UI symbols for `deui_ui.c` and per-mode screen units (`deui_ui_screen_*.c`).
 * Not a public API — do not include from other firmware modules.
 */
#pragma once

#include "lvgl.h"

extern lv_obj_t *deui_ui_obj_machine_state;
extern lv_obj_t *deui_ui_obj_metrics_card;

/**
 * When 1: any BLE-connected DE1 uses brewing chrome (`deui_ui_screen_apply_brewing` + shot capsule/arcs).
 * `deui_ui_screen_idle.c` stays built but is not called from `deui_ui_update_status` until Espresso gating is reliable.
 */
#ifndef DEUI_UI_TEMP_ALWAYS_BREWING_WHEN_CONNECTED
#define DEUI_UI_TEMP_ALWAYS_BREWING_WHEN_CONNECTED 1
#endif

void deui_ui_label_set_static_if_changed(lv_obj_t *label, const char *txt);
