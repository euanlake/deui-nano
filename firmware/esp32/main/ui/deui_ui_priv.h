/**
 * Shared UI symbols for `deui_ui.c` and per-mode screen units (`deui_ui_screen_*.c`).
 * Not a public API — do not include from other firmware modules.
 */
#pragma once

#include "lvgl.h"

extern lv_obj_t *deui_ui_obj_machine_state;
extern lv_obj_t *deui_ui_obj_metrics_card;
extern lv_obj_t *deui_ui_obj_footer;

/** Set to 1 only for temporary “always brewing chrome when connected” bring-up. */
#ifndef DEUI_UI_TEMP_ALWAYS_BREWING_WHEN_CONNECTED
#define DEUI_UI_TEMP_ALWAYS_BREWING_WHEN_CONNECTED 0
#endif

/** Vertical stack on round 360×360: metrics capsule → idle headline → footer link icons. */
enum {
  k_metrics_card_center_y_ofs = -10,
  k_idle_headline_below_metrics_gap = 6,
  k_footer_icons_bottom_inset = 14,
};

void deui_ui_label_set_static_if_changed(lv_obj_t *label, const char *txt);
