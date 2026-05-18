#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

void deui_ui_set_arc_marker(lv_obj_t *arc, int value_cent, int max_cent);
void deui_ui_set_textf(lv_obj_t *label, const char *fmt, ...);
void deui_ui_strip_default_theme(lv_obj_t *obj);
void deui_ui_pin_label_no_theme_recolor(lv_obj_t *obj);
void deui_ui_status_icon_apply(lv_obj_t *icon, bool pulse, uint32_t primary_text, uint32_t subtle_text,
                                bool pulse_phase);
