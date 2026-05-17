/**
 * DE1 major **Espresso** (0x04) only: hide headline, show metrics capsule + arcs (via `deui_ui_update_metrics`).
 * Mutually exclusive with idle/searching; selection is done in `deui_ui_update_status`.
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
  lv_obj_add_flag(deui_ui_obj_machine_state, LV_OBJ_FLAG_HIDDEN);
}
