/**
 * Live extraction: hide headline, show metrics capsule (+ arcs when `shot_layout` is true).
 * Routed only while DE1 major Espresso (0x04) and an active extraction minor.
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
