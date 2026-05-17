/**
 * Connected, not in DE1 **Espresso** major (0x04): headline + metrics grid (transparent plate).
 * Mutually exclusive with `deui_ui_screen_brewing` / `deui_ui_screen_searching`.
 */
#include "deui_ui_priv.h"
#include "deui_ui_screens.h"

void deui_ui_screen_apply_idle(const char *machine_state_center) {
  if (deui_ui_obj_metrics_card != NULL) {
    lv_obj_clear_flag(deui_ui_obj_metrics_card, LV_OBJ_FLAG_HIDDEN);
  }
  if (deui_ui_obj_machine_state == NULL || machine_state_center == NULL) {
    return;
  }
  lv_obj_align(deui_ui_obj_machine_state, LV_ALIGN_CENTER, 0, -78);
  const char *idle =
      machine_state_center[0] != '\0' ? machine_state_center : "Waiting...";
  deui_ui_label_set_static_if_changed(deui_ui_obj_machine_state, idle);
  lv_obj_clear_flag(deui_ui_obj_machine_state, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(deui_ui_obj_machine_state);
}
