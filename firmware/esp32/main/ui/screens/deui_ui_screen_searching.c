#include "deui_ui_priv.h"
#include "deui_ui_screens.h"

/** Above geometric center — bottom status icons add visual weight and pull focus downward. */
enum { k_searching_headline_y = -28 };

void deui_ui_screen_apply_searching(void) {
  if (deui_ui_obj_metrics_card != NULL) {
    lv_obj_add_flag(deui_ui_obj_metrics_card, LV_OBJ_FLAG_HIDDEN);
  }
  if (deui_ui_obj_machine_state == NULL) {
    return;
  }
  lv_obj_align(deui_ui_obj_machine_state, LV_ALIGN_CENTER, 0, k_searching_headline_y);
  deui_ui_label_set_static_if_changed(deui_ui_obj_machine_state, "Searching");
  lv_obj_clear_flag(deui_ui_obj_machine_state, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(deui_ui_obj_machine_state);
  if (deui_ui_obj_footer != NULL) {
    lv_obj_add_flag(deui_ui_obj_footer, LV_OBJ_FLAG_HIDDEN);
  }
}
