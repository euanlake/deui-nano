/**
 * LVGL layout for each high-level product mode (one physical screen, three visibility/chrome modes).
 */
#pragma once

void deui_ui_screen_apply_searching(void);

/** `machine_state_center` may be empty; caller guarantees pointer is non-NULL. */
void deui_ui_screen_apply_idle(const char *machine_state_center);

void deui_ui_screen_apply_brewing(void);

/** Connected, major state is neither Idle (0x02) nor Espresso (0x04). */
void deui_ui_screen_apply_status(const char *machine_state_center);
