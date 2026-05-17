/**
 * Connected to DE1 but not in Idle (0x02) or Espresso (0x04): headline only (Steam, Busy, …).
 */
#include "deui_ui_priv.h"
#include "deui_ui_screens.h"

void deui_ui_screen_apply_status(const char *machine_state_center) {
  if (deui_ui_obj_metrics_card != NULL) {
    lv_obj_add_flag(deui_ui_obj_metrics_card, LV_OBJ_FLAG_HIDDEN);
  }
  if (deui_ui_obj_machine_state == NULL || machine_state_center == NULL) {
    return;
  }
  lv_obj_align(deui_ui_obj_machine_state, LV_ALIGN_CENTER, 0, 0);
  const char *text =
      machine_state_center[0] != '\0' ? machine_state_center : "Connected";
  deui_ui_label_set_static_if_changed(deui_ui_obj_machine_state, text);
  lv_obj_clear_flag(deui_ui_obj_machine_state, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(deui_ui_obj_machine_state);
}
