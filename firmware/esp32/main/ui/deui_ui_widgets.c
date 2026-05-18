#include "deui_ui_internal.h"

void deui_ui_pin_label_no_theme_recolor(lv_obj_t *obj) {
  if (obj == NULL) {
    return;
  }
  lv_obj_set_style_color_filter_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_text_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
}

void deui_ui_strip_default_theme(lv_obj_t *obj) {
  lv_obj_remove_style_all(obj);
}

void deui_ui_status_icon_apply(lv_obj_t *icon, bool pulse, uint32_t primary_text, uint32_t subtle_text,
                                bool pulse_phase) {
  if (icon == NULL) {
    return;
  }
  if (pulse) {
    lv_color_t c = pulse_phase ? lv_color_hex(primary_text) : lv_color_hex(subtle_text);
    lv_obj_set_style_text_color(icon, c, LV_PART_MAIN);
    lv_obj_set_style_text_opa(icon, LV_OPA_COVER, LV_PART_MAIN);
  } else {
    lv_obj_set_style_text_color(icon, lv_color_hex(primary_text), LV_PART_MAIN);
    lv_obj_set_style_text_opa(icon, LV_OPA_COVER, LV_PART_MAIN);
  }
  lv_obj_set_style_color_filter_opa(icon, LV_OPA_TRANSP, LV_PART_MAIN);
}
