/**
 * Brewing presentation: hide headline, show metrics capsule (+ arcs when `shot_layout` is true).
 * Routed for every connected DE1 while `DEUI_UI_TEMP_ALWAYS_BREWING_WHEN_CONNECTED`; otherwise Espresso (0x04) only.
 */
#include "deui_ui_priv.h"
#include "deui_ui_screens.h"

void deui_ui_screen_apply_brewing(void) {
  if (deui_ui_obj_metrics_card != NULL) {
    lv_obj_clear_flag(deui_ui_obj_metrics_card, LV_OBJ_FLAG_HIDDEN);
  }
  if (deui_ui_obj_machine_state == NULL) {
    return;
  }
  deui_ui_label_set_static_if_changed(deui_ui_obj_machine_state, "");
  lv_obj_add_flag(deui_ui_obj_machine_state, LV_OBJ_FLAG_HIDDEN);
}
