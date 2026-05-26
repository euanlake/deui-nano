/**
 * Connected to DE1 but not in active extraction: machine-state headline.
 * Post-shot saved peaks use the metrics grid when `show_saved_metrics` is true.
 */
#include "deui_ui_priv.h"
#include "deui_ui_screens.h"

LV_FONT_DECLARE(LabGrotesque_Bold_24);

/** Gap below the metrics capsule before the idle headline (Ready, Heating, …). */
enum { k_idle_headline_below_metrics_gap = 8 };

void deui_ui_screen_apply_idle(const char *machine_state_center, bool show_saved_metrics) {
  (void)show_saved_metrics;

  if (deui_ui_obj_metrics_card != NULL) {
    lv_obj_clear_flag(deui_ui_obj_metrics_card, LV_OBJ_FLAG_HIDDEN);
  }
  if (deui_ui_obj_machine_state == NULL || machine_state_center == NULL) {
    return;
  }
  lv_obj_set_style_text_font(deui_ui_obj_machine_state, &LabGrotesque_Bold_24, LV_PART_MAIN);
  if (deui_ui_obj_metrics_card != NULL) {
    lv_obj_align_to(deui_ui_obj_machine_state, deui_ui_obj_metrics_card, LV_ALIGN_OUT_BOTTOM_MID, 0,
                    k_idle_headline_below_metrics_gap);
  } else {
    lv_obj_align(deui_ui_obj_machine_state, LV_ALIGN_CENTER, 0, 120);
  }
  const char *idle =
      machine_state_center[0] != '\0' ? machine_state_center : "Waiting...";
  deui_ui_label_set_static_if_changed(deui_ui_obj_machine_state, idle);
  lv_obj_clear_flag(deui_ui_obj_machine_state, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(deui_ui_obj_machine_state);
}
