/**
 * LVGL layout for each high-level product mode (one physical screen, three visibility/chrome modes).
 */
#pragma once

#include <stdbool.h>

void deui_ui_screen_apply_searching(void);

/** `machine_state_center` may be empty; caller guarantees pointer is non-NULL. */
void deui_ui_screen_apply_idle(const char *machine_state_center, bool show_saved_metrics);

void deui_ui_screen_apply_brewing(void);
