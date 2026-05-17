/**
 * DE1 major **Idle** (0x02) only: headline + metrics grid (transparent plate).
 * Mutually exclusive with brewing, status, and searching screens.
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
