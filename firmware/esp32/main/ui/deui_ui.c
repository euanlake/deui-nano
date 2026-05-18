#include "deui_ui.h"

#include <stdio.h>
#include <string.h>

#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_config.h"
#include "deui_ble_client.h"
#include "deui_shot_metrics.h"
#include "deui_theme.h"
#include "deui_ui_internal.h"
#include "deui_ui_priv.h"
#include "deui_ui_screens.h"
#include "esp_lv_adapter.h"

#include "lvgl.h"

lv_obj_t *deui_ui_obj_machine_state = NULL;
lv_obj_t *deui_ui_obj_metrics_card = NULL;

LV_FONT_DECLARE(LabGrotesque_Regular_16);
LV_FONT_DECLARE(LabGrotesque_Bold_48);
LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_40);

static lv_obj_t *s_footer = NULL;
static lv_obj_t *s_weight_label = NULL;
static lv_obj_t *s_weight_value = NULL;
static lv_obj_t *s_time_label = NULL;
static lv_obj_t *s_time_value = NULL;
static lv_obj_t *s_weight_col = NULL;
static lv_obj_t *s_time_col = NULL;
static lv_obj_t *s_pressure_col = NULL;
static lv_obj_t *s_flow_col = NULL;
static lv_obj_t *s_pressure_label = NULL;
static lv_obj_t *s_pressure_value = NULL;
static lv_obj_t *s_flow_label = NULL;
static lv_obj_t *s_flow_value = NULL;
/** LVGL FontAwesome symbols — lower round "safe" zone, away from clipped top corners. */
static lv_obj_t *s_ble_icon = NULL;
static lv_obj_t *s_wifi_icon = NULL;
static lv_obj_t *s_scale_icon = NULL;
static lv_obj_t *s_usb = NULL;
static lv_obj_t *s_battery = NULL;
static lv_obj_t *s_flow_arc = NULL;
static lv_obj_t *s_pressure_arc = NULL;

static bool s_ble_icon_pulse = true;
static bool s_wifi_icon_pulse = true;
static bool s_scale_icon_pulse = true;
static bool s_link_pulse_phase = false;
static int64_t s_link_next_pulse_us = 0;

static uint32_t s_color_primary_text = 0xebe8e8;
static uint32_t s_color_subtle_fg = 0x757575;

static deui_theme_mode_t s_theme_mode = DEUI_THEME_MODE_DARK;

/** True: solid metrics “capsule” while pulling a shot; false: transparent plate under idle/search + zeroed grid. */
static bool s_metrics_shot_layout = false;
/** True: live extraction values (strong text); false: session peaks / saved post-shot (subtle text). */
static bool s_metrics_live = false;


static const lv_font_t *font_regular_16(void) {
  return &LabGrotesque_Regular_16;
}

static const lv_font_t *font_value_48(void) {
  return &LabGrotesque_Bold_48;
}

/** Metrics grid numerals — built-in size, no transform (see file comment above). */
static const lv_font_t *font_metric_value(void) {
  return &lv_font_montserrat_40;
}

/**
 * Shot metric **values** must not use transform zoom: with ESP-IDF 5.x SPI partial flushes, zoomed layers
 * often fail to repaint while captions (no zoom) still draw — looks like “labels but no numbers”.
 * Idle headline stays on Bold 48 at default zoom (zoom 256).
 */
enum {
  /**
   * Arc value = physical × 100 (matches on-screen decimals): flow ml/s, pressure bar.
   * maxima must stay in sync with `lv_arc_set_range` in deui_ui_init.
   */
  k_flow_arc_max = 1200,
  k_pressure_arc_max = 1200,
  /** Draw a short marker centered on the value (0.5 "number" wide = +/-0.25). */
  k_arc_marker_half_width = 25,
  /** Arc value space for one full clock-face revolution (12.00 units). */
  k_arc_revolution = 1200,
  /** Indicator value used to fully fill the current marker span. */
  k_arc_marker_fill = 100,
};

/** Flow/pressure ring colours and track: always from `deui_theme.h` palette (flow_arc, pressure_arc, arc_track). */
static void style_shot_arcs_from_theme(const deui_theme_palette_t *theme) {
  if (theme == NULL) {
    return;
  }
  if (s_flow_arc != NULL) {
    lv_obj_set_style_arc_color(s_flow_arc, lv_color_hex(theme->flow_arc), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_flow_arc, lv_color_hex(theme->arc_track), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_flow_arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s_flow_arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_flow_arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_flow_arc, LV_OPA_TRANSP, LV_PART_KNOB);
  }
  if (s_pressure_arc != NULL) {
    lv_obj_set_style_arc_color(s_pressure_arc, lv_color_hex(theme->pressure_arc), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_pressure_arc, lv_color_hex(theme->arc_track), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_pressure_arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s_pressure_arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_pressure_arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_pressure_arc, LV_OPA_TRANSP, LV_PART_KNOB);
  }
}

static void apply_metric_labels(bool live) {
  if (s_weight_label != NULL) {
    deui_ui_label_set_static_if_changed(s_weight_label, live ? "WEIGHT (G)" : "MAX WEIGHT (G)");
  }
  if (s_time_label != NULL) {
    deui_ui_label_set_static_if_changed(s_time_label, live ? "TIME (S)" : "MAX TIME (S)");
  }
  if (s_pressure_label != NULL) {
    deui_ui_label_set_static_if_changed(s_pressure_label, live ? "PRESS (BAR)" : "MAX PRESS (BAR)");
  }
  if (s_flow_label != NULL) {
    deui_ui_label_set_static_if_changed(s_flow_label, live ? "FLOW (ML/S)" : "MAX FLOW (ML/S)");
  }
}

static void apply_metric_value_colors(bool live) {
  const uint32_t value_color = live ? s_color_primary_text : s_color_subtle_fg;
  if (s_weight_value != NULL) {
    lv_obj_set_style_text_color(s_weight_value, lv_color_hex(value_color), LV_PART_MAIN);
  }
  if (s_time_value != NULL) {
    lv_obj_set_style_text_color(s_time_value, lv_color_hex(value_color), LV_PART_MAIN);
  }
  if (s_pressure_value != NULL) {
    lv_obj_set_style_text_color(s_pressure_value, lv_color_hex(value_color), LV_PART_MAIN);
  }
  if (s_flow_value != NULL) {
    lv_obj_set_style_text_color(s_flow_value, lv_color_hex(value_color), LV_PART_MAIN);
  }
}

static void sync_metrics_card_chrome(const deui_theme_palette_t *theme) {
  if (deui_ui_obj_metrics_card == NULL || theme == NULL) {
    return;
  }
  if (s_metrics_shot_layout) {
    lv_obj_set_style_bg_color(deui_ui_obj_metrics_card, lv_color_hex(theme->card_bg), LV_PART_MAIN);
    /** Keep capsule readable but translucent so shot arcs stay visible beneath it. */
    lv_obj_set_style_bg_opa(deui_ui_obj_metrics_card, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_width(deui_ui_obj_metrics_card, theme->card_border_width, LV_PART_MAIN);
    lv_obj_set_style_border_color(deui_ui_obj_metrics_card, lv_color_hex(theme->card_border), LV_PART_MAIN);
  } else {
    lv_obj_set_style_bg_opa(deui_ui_obj_metrics_card, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(deui_ui_obj_metrics_card, 0, LV_PART_MAIN);
  }
}

static void apply_theme_palette(const deui_theme_palette_t *theme) {
  lv_obj_t *root = lv_scr_act();
  lv_obj_set_style_bg_color(root, lv_color_hex(theme->screen_bg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
  /** Screen-level text hint for children; we still set each label explicitly. */
  lv_obj_set_style_text_color(root, lv_color_hex(theme->primary_text), LV_PART_MAIN);

  lv_disp_t *disp = lv_disp_get_default();
  if (disp != NULL) {
    lv_disp_set_bg_color(disp, lv_color_hex(theme->screen_bg));
    lv_disp_set_bg_opa(disp, LV_OPA_COVER);
  }

  s_color_primary_text = theme->primary_text;
  s_color_subtle_fg = theme->subtle_text;

  style_shot_arcs_from_theme(theme);
  if (s_weight_label != NULL) {
    lv_obj_set_style_text_color(s_weight_label, lv_color_hex(theme->subtle_text), LV_PART_MAIN);
  }
  if (s_time_label != NULL) {
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(theme->subtle_text), LV_PART_MAIN);
  }
  if (s_pressure_label != NULL) {
    lv_obj_set_style_text_color(s_pressure_label, lv_color_hex(theme->subtle_text), LV_PART_MAIN);
  }
  if (s_flow_label != NULL) {
    lv_obj_set_style_text_color(s_flow_label, lv_color_hex(theme->subtle_text), LV_PART_MAIN);
  }
  apply_metric_value_colors(s_metrics_live);
  if (deui_ui_obj_machine_state != NULL) {
    lv_obj_set_style_text_color(deui_ui_obj_machine_state, lv_color_hex(theme->primary_text), LV_PART_MAIN);
  }
  if (s_ble_icon != NULL) {
    lv_obj_set_style_text_color(s_ble_icon, lv_color_hex(theme->primary_text), LV_PART_MAIN);
    lv_obj_set_style_color_filter_opa(s_ble_icon, LV_OPA_TRANSP, LV_PART_MAIN);
  }
  if (s_wifi_icon != NULL) {
    lv_obj_set_style_text_color(s_wifi_icon, lv_color_hex(theme->primary_text), LV_PART_MAIN);
    lv_obj_set_style_color_filter_opa(s_wifi_icon, LV_OPA_TRANSP, LV_PART_MAIN);
  }
  if (s_scale_icon != NULL) {
    lv_obj_set_style_text_color(s_scale_icon, lv_color_hex(theme->primary_text), LV_PART_MAIN);
    lv_obj_set_style_color_filter_opa(s_scale_icon, LV_OPA_TRANSP, LV_PART_MAIN);
  }
  if (s_footer != NULL) {
    lv_obj_set_style_text_color(s_footer, lv_color_hex(theme->subtle_text), LV_PART_MAIN);
  }
  if (s_usb != NULL) {
    lv_obj_set_style_text_color(s_usb, lv_color_hex(theme->subtle_text), LV_PART_MAIN);
  }
  if (s_battery != NULL) {
    lv_obj_set_style_text_color(s_battery, lv_color_hex(theme->subtle_text), LV_PART_MAIN);
  }
  sync_metrics_card_chrome(theme);

  deui_ui_pin_label_no_theme_recolor(s_weight_label);
  deui_ui_pin_label_no_theme_recolor(s_weight_value);
  deui_ui_pin_label_no_theme_recolor(s_time_label);
  deui_ui_pin_label_no_theme_recolor(s_time_value);
  deui_ui_pin_label_no_theme_recolor(s_pressure_label);
  deui_ui_pin_label_no_theme_recolor(s_pressure_value);
  deui_ui_pin_label_no_theme_recolor(s_flow_label);
  deui_ui_pin_label_no_theme_recolor(s_flow_value);
  deui_ui_pin_label_no_theme_recolor(deui_ui_obj_machine_state);
  deui_ui_pin_label_no_theme_recolor(s_ble_icon);
  deui_ui_pin_label_no_theme_recolor(s_wifi_icon);
  deui_ui_pin_label_no_theme_recolor(s_scale_icon);
  deui_ui_pin_label_no_theme_recolor(s_footer);
  deui_ui_pin_label_no_theme_recolor(s_usb);
  deui_ui_pin_label_no_theme_recolor(s_battery);
}

static void create_shot_metric_cell(lv_obj_t *card, lv_obj_t **out_col, lv_obj_t **out_lbl, lv_obj_t **out_val,
                                    const char *caption, lv_coord_t cell_w, lv_coord_t cell_h, lv_align_t align,
                                    const deui_theme_palette_t *theme) {
  *out_col = lv_obj_create(card);
  deui_ui_strip_default_theme(*out_col);
  lv_obj_set_size(*out_col, cell_w, cell_h);
  lv_obj_align(*out_col, align, 0, 0);
  lv_obj_add_flag(*out_col, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_style_bg_opa(*out_col, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(*out_col, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(*out_col, 0, LV_PART_MAIN);

  *out_lbl = lv_label_create(*out_col);
  deui_ui_strip_default_theme(*out_lbl);
  lv_obj_add_flag(*out_lbl, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_style_bg_opa(*out_lbl, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_label_set_text(*out_lbl, caption);
  /** Full cell width + CLIP so captions/values center in the quadrant (content-sized labels looked right-shifted). */
  lv_obj_set_width(*out_lbl, cell_w);
  lv_label_set_long_mode(*out_lbl, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_color(*out_lbl, lv_color_hex(theme->subtle_text), LV_PART_MAIN);
  lv_obj_set_style_text_font(*out_lbl, font_regular_16(), LV_PART_MAIN);
  lv_obj_set_style_text_align(*out_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(*out_lbl, LV_ALIGN_TOP_MID, 0, 0);

  *out_val = lv_label_create(*out_col);
  deui_ui_strip_default_theme(*out_val);
  lv_obj_add_flag(*out_val, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_style_bg_opa(*out_val, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_label_set_text(*out_val, "0");
  lv_obj_set_width(*out_val, cell_w);
  lv_label_set_long_mode(*out_val, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_color(*out_val, lv_color_hex(theme->primary_text), LV_PART_MAIN);
  lv_obj_set_style_text_font(*out_val, font_metric_value(), LV_PART_MAIN);
  lv_obj_set_style_text_align(*out_val, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  /** Sit value directly under caption — avoids large TOP/BOTTOM gap inside the cell. */
  lv_obj_align_to(*out_val, *out_lbl, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
  deui_ui_pin_label_no_theme_recolor(*out_lbl);
  deui_ui_pin_label_no_theme_recolor(*out_val);

  /** `deui_ui_init` runs before the LVGL tick loop; give IDLE (and the task WDT) time on each cell. */
  vTaskDelay(1);
}

/** Skip lv_label realloc / WRAP relayout when string unchanged — keeps heap quieter and IDLE fed. */
void deui_ui_label_set_static_if_changed(lv_obj_t *label, const char *txt) {
  if (label == NULL || txt == NULL) {
    return;
  }
  if (strcmp(lv_label_get_text(label), txt) == 0) {
    return;
  }
  lv_label_set_text(label, txt);
}

/**
 * Rotary / swipe: delta > 0 → light theme, delta < 0 → dark.
 * Caller must already hold `esp_lv_adapter_lock` (LVGL worker invokes gestures under that lock).
 */
static void deui_ui_theme_nudge_locked(int delta) {
  bool to_light = (delta > 0 && s_theme_mode == DEUI_THEME_MODE_DARK);
  bool to_dark = (delta < 0 && s_theme_mode == DEUI_THEME_MODE_LIGHT);
  if (to_light) {
    s_theme_mode = DEUI_THEME_MODE_LIGHT;
  } else if (to_dark) {
    s_theme_mode = DEUI_THEME_MODE_DARK;
  }
  if (to_light || to_dark) {
    deui_theme_palette_t theme;
    deui_theme_palette_for_mode(s_theme_mode, &theme);
    s_color_primary_text = theme.primary_text;
    apply_theme_palette(&theme);
    deui_ui_status_icon_apply(s_ble_icon, s_ble_icon_pulse, s_color_primary_text, s_color_subtle_fg,
                              s_link_pulse_phase);
    deui_ui_status_icon_apply(s_wifi_icon, s_wifi_icon_pulse, s_color_primary_text, s_color_subtle_fg,
                              s_link_pulse_phase);
    if (s_scale_icon_pulse) {
      deui_ui_status_icon_apply(s_scale_icon, true, s_color_primary_text, s_color_subtle_fg, s_link_pulse_phase);
    } else {
      deui_ui_status_icon_apply(s_scale_icon, false, s_color_primary_text, s_color_subtle_fg, s_link_pulse_phase);
    }
  }
}

/** Touch: swipe changes theme (same mapping as rotary — right/bottom → lighter, left/top → darker). */
static void root_gesture_theme_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_GESTURE) {
    return;
  }
  lv_indev_t *indev = lv_indev_get_act();
  if (indev == NULL) {
    return;
  }
  lv_dir_t dir = lv_indev_get_gesture_dir(indev);
  int delta = 0;
  if (dir == LV_DIR_RIGHT || dir == LV_DIR_BOTTOM) {
    delta = 1;
  } else if (dir == LV_DIR_LEFT || dir == LV_DIR_TOP) {
    delta = -1;
  }
  if (delta == 0) {
    return;
  }
  lv_indev_wait_release(indev);
  deui_ui_theme_nudge_locked(delta);
}

/**
 * LVGL APIs are not safe to call concurrently with esp_lv_adapter's worker (which owns lv_timer_handler()).
 * All UI setup and updates must run under esp_lv_adapter_lock(); app_main must not call lv_timer_handler().
 */
static esp_err_t deui_ui_init_under_lock(lv_disp_t *display) {
  (void)display;
  lv_coord_t lcd_w = (lv_coord_t)LM_CTRL_LCD_H_RES;

  deui_theme_palette_t theme;
  deui_theme_palette_for_mode(s_theme_mode, &theme);
  s_color_primary_text = theme.primary_text;
  s_color_subtle_fg = theme.subtle_text;

  lv_obj_t *root = lv_scr_act();
  lv_obj_set_style_bg_color(root, lv_color_hex(theme.screen_bg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_text_color(root, lv_color_hex(theme.primary_text), LV_PART_MAIN);

  lv_disp_t *disp = display != NULL ? display : lv_disp_get_default();
  if (disp != NULL) {
    lv_disp_set_bg_color(disp, lv_color_hex(theme.screen_bg));
    lv_disp_set_bg_opa(disp, LV_OPA_COVER);
    /**
     * LVGL registers a default theme with the display (pink/cyan accents). Remove it so our widgets are not
     * re-tinted after `lv_obj_remove_style_all` / relayout.
     */
    lv_disp_set_theme(disp, NULL);
  }

  /*
   * Shot arcs are full-panel siblings of the metrics card. Z-order is controlled in
   * `deui_ui_update_status`: during brewing we move arcs above the card so they are not clipped.
   * Colours: deui_theme_palette_t.flow_arc / pressure_arc / arc_track only.
   */
  const lv_coord_t flow_arc_sz = lcd_w - 8;
  const lv_coord_t pressure_arc_sz = lcd_w - 24;

  s_flow_arc = lv_arc_create(root);
  deui_ui_strip_default_theme(s_flow_arc);
  lv_obj_set_size(s_flow_arc, flow_arc_sz, flow_arc_sz);
  lv_obj_align(s_flow_arc, LV_ALIGN_CENTER, 0, 0);
  lv_obj_clear_flag(s_flow_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_arc_set_bg_angles(s_flow_arc, 262, 278);
  lv_arc_set_mode(s_flow_arc, LV_ARC_MODE_NORMAL);
  lv_arc_set_range(s_flow_arc, 0, k_arc_marker_fill);
  lv_arc_set_value(s_flow_arc, k_arc_marker_fill);
  lv_obj_set_style_arc_width(s_flow_arc, 10, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(s_flow_arc, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_flow_arc, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_flow_arc, 0, LV_PART_KNOB);

  s_pressure_arc = lv_arc_create(root);
  deui_ui_strip_default_theme(s_pressure_arc);
  lv_obj_set_size(s_pressure_arc, pressure_arc_sz, pressure_arc_sz);
  lv_obj_align(s_pressure_arc, LV_ALIGN_CENTER, 0, 0);
  lv_obj_clear_flag(s_pressure_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_arc_set_bg_angles(s_pressure_arc, 262, 278);
  lv_arc_set_range(s_pressure_arc, 0, k_arc_marker_fill);
  lv_arc_set_value(s_pressure_arc, k_arc_marker_fill);
  lv_obj_set_style_arc_width(s_pressure_arc, 10, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(s_pressure_arc, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_pressure_arc, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_pressure_arc, 0, LV_PART_KNOB);

  style_shot_arcs_from_theme(&theme);

  lv_obj_move_background(s_pressure_arc);
  lv_obj_move_background(s_flow_arc);
  lv_obj_add_flag(s_flow_arc, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(s_pressure_arc, LV_OBJ_FLAG_HIDDEN);

  const lv_coord_t card_w = LV_MIN(lcd_w - 32, 320);
  const lv_coord_t card_h = 234;

  deui_ui_obj_metrics_card = lv_obj_create(root);
  deui_ui_strip_default_theme(deui_ui_obj_metrics_card);
  /** Default widgets are LV_OBJ_FLAG_SCROLLABLE; clip + transform-zoom labels can omit pixels (e.g. "Searching"). */
  lv_obj_clear_flag(deui_ui_obj_metrics_card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(deui_ui_obj_metrics_card, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_size(deui_ui_obj_metrics_card, card_w, card_h);
  lv_obj_align(deui_ui_obj_metrics_card, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(deui_ui_obj_metrics_card, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_all(deui_ui_obj_metrics_card, 14, LV_PART_MAIN);
  /** Rounded-rect clip was cutting off caption/value glyphs at the capsule edge. */
  lv_obj_set_style_clip_corner(deui_ui_obj_metrics_card, false, LV_PART_MAIN);
  sync_metrics_card_chrome(&theme);

  const lv_coord_t pad = 14;
  lv_coord_t inner_w = card_w - 2 * pad;
  lv_coord_t inner_h = card_h - 2 * pad;
  const lv_coord_t gap = 4;
  lv_coord_t cell_w = (inner_w - gap) / 2;
  lv_coord_t cell_h = (inner_h - gap) / 2;

  /**
   * 2×2 grid (left→right, top→bottom): weight | time, then pressure | flow.
   * Corners anchor each cell inside the padded card.
   */
  create_shot_metric_cell(deui_ui_obj_metrics_card, &s_weight_col, &s_weight_label, &s_weight_value, "WEIGHT (G)", cell_w,
                           cell_h, LV_ALIGN_TOP_LEFT, &theme);
  create_shot_metric_cell(deui_ui_obj_metrics_card, &s_time_col, &s_time_label, &s_time_value, "TIME (S)", cell_w, cell_h,
                          LV_ALIGN_TOP_RIGHT, &theme);
  create_shot_metric_cell(deui_ui_obj_metrics_card, &s_pressure_col, &s_pressure_label, &s_pressure_value, "PRESS (BAR)",
                          cell_w, cell_h, LV_ALIGN_BOTTOM_LEFT, &theme);
  create_shot_metric_cell(deui_ui_obj_metrics_card, &s_flow_col, &s_flow_label, &s_flow_value, "FLOW (ML/S)", cell_w, cell_h,
                          LV_ALIGN_BOTTOM_RIGHT, &theme);

  /**
   * Searching / Idle headline stays on `root`, not inside `deui_ui_obj_metrics_card` (card clip + scroll once hid glyphs).
   * Sits above the metrics block so the 2×2 grid can stay visible with zeros while disconnected / idle.
   */
  deui_ui_obj_machine_state = lv_label_create(root);
  deui_ui_strip_default_theme(deui_ui_obj_machine_state);
  lv_obj_add_flag(deui_ui_obj_machine_state, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_width(deui_ui_obj_machine_state, card_w - 24);
  lv_label_set_long_mode(deui_ui_obj_machine_state, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_bg_opa(deui_ui_obj_machine_state, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_text_font(deui_ui_obj_machine_state, font_value_48(), LV_PART_MAIN);
  lv_obj_set_style_text_color(deui_ui_obj_machine_state, lv_color_hex(theme.primary_text), LV_PART_MAIN);
  lv_obj_set_style_text_align(deui_ui_obj_machine_state, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(deui_ui_obj_machine_state, "");
  /** Idle: shifted up so the metric block fits below; Searching mode re-centers in `deui_ui_screen_searching`. */
  lv_obj_align(deui_ui_obj_machine_state, LV_ALIGN_CENTER, 0, -78);
  lv_obj_add_flag(deui_ui_obj_machine_state, LV_OBJ_FLAG_HIDDEN);
  deui_ui_pin_label_no_theme_recolor(deui_ui_obj_machine_state);

  /*
   * Round panel: icons sit just below the metrics card (taller 2×2 shot grid).
   */
  s_ble_icon = lv_label_create(root);
  deui_ui_strip_default_theme(s_ble_icon);
  lv_label_set_text(s_ble_icon, LV_SYMBOL_BLUETOOTH);
  lv_obj_set_style_text_font(s_ble_icon, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(s_ble_icon, lv_color_hex(theme.primary_text), LV_PART_MAIN);
  lv_obj_set_style_color_filter_opa(s_ble_icon, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_text_align(s_ble_icon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(s_ble_icon, LV_ALIGN_CENTER, -34, 133);

  s_wifi_icon = lv_label_create(root);
  deui_ui_strip_default_theme(s_wifi_icon);
  lv_label_set_text(s_wifi_icon, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_font(s_wifi_icon, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(s_wifi_icon, lv_color_hex(theme.primary_text), LV_PART_MAIN);
  lv_obj_set_style_color_filter_opa(s_wifi_icon, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_text_align(s_wifi_icon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(s_wifi_icon, LV_ALIGN_CENTER, 56, 133);

  s_scale_icon = lv_label_create(root);
  deui_ui_strip_default_theme(s_scale_icon);
  /** No dedicated LVGL "scale" glyph in built-ins; `DRIVE` reads well as a compact scale-like icon. */
  lv_label_set_text(s_scale_icon, LV_SYMBOL_DRIVE);
  lv_obj_set_style_text_font(s_scale_icon, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(s_scale_icon, lv_color_hex(theme.primary_text), LV_PART_MAIN);
  lv_obj_set_style_color_filter_opa(s_scale_icon, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_text_align(s_scale_icon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(s_scale_icon, LV_ALIGN_CENTER, 0, 133);
  lv_obj_align(s_ble_icon, LV_ALIGN_CENTER, -56, 133);
  deui_ui_pin_label_no_theme_recolor(s_ble_icon);
  deui_ui_pin_label_no_theme_recolor(s_wifi_icon);
  deui_ui_pin_label_no_theme_recolor(s_scale_icon);

  s_footer = lv_label_create(root);
  deui_ui_strip_default_theme(s_footer);
  lv_obj_set_style_bg_opa(s_footer, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_width(s_footer, lcd_w - 40);
  lv_label_set_long_mode(s_footer, LV_LABEL_LONG_WRAP);
  lv_obj_align(s_footer, LV_ALIGN_BOTTOM_MID, 0, -50);
  lv_obj_set_style_text_color(s_footer, lv_color_hex(theme.subtle_text), LV_PART_MAIN);
  lv_obj_set_style_text_align(s_footer, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_font(s_footer, font_regular_16(), LV_PART_MAIN);
  lv_label_set_text(s_footer, "");
  lv_obj_add_flag(s_footer, LV_OBJ_FLAG_HIDDEN);

  s_usb = lv_label_create(root);
  deui_ui_strip_default_theme(s_usb);
  lv_obj_set_style_bg_opa(s_usb, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_align(s_usb, LV_ALIGN_TOP_RIGHT, -62, 6);
  lv_obj_set_style_text_font(s_usb, font_regular_16(), LV_PART_MAIN);
  lv_obj_set_style_text_color(s_usb, lv_color_hex(theme.subtle_text), LV_PART_MAIN);
  lv_label_set_text(s_usb, "USB");
  lv_obj_add_flag(s_usb, LV_OBJ_FLAG_HIDDEN);

  s_battery = lv_label_create(root);
  deui_ui_strip_default_theme(s_battery);
  lv_obj_set_style_bg_opa(s_battery, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_align(s_battery, LV_ALIGN_TOP_RIGHT, -10, 6);
  lv_obj_set_style_text_font(s_battery, font_regular_16(), LV_PART_MAIN);
  lv_obj_set_style_text_color(s_battery, lv_color_hex(theme.subtle_text), LV_PART_MAIN);
  lv_label_set_text(s_battery, "PWR");
  lv_obj_add_flag(s_battery, LV_OBJ_FLAG_HIDDEN);

  deui_ui_pin_label_no_theme_recolor(s_footer);
  deui_ui_pin_label_no_theme_recolor(s_usb);
  deui_ui_pin_label_no_theme_recolor(s_battery);

  s_link_next_pulse_us = esp_timer_get_time();
  deui_ui_status_icon_apply(s_ble_icon, s_ble_icon_pulse, s_color_primary_text, s_color_subtle_fg,
                            s_link_pulse_phase);
  deui_ui_status_icon_apply(s_wifi_icon, s_wifi_icon_pulse, s_color_primary_text, s_color_subtle_fg,
                            s_link_pulse_phase);
  deui_ui_status_icon_apply(s_scale_icon, s_scale_icon_pulse, s_color_primary_text, s_color_subtle_fg,
                            s_link_pulse_phase);

  if (deui_ui_obj_machine_state != NULL) {
    lv_obj_move_foreground(deui_ui_obj_machine_state);
  }

  lv_obj_clear_flag(root, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(root, root_gesture_theme_cb, LV_EVENT_GESTURE, NULL);

  deui_ui_screen_apply_searching();

  return ESP_OK;
}

esp_err_t deui_ui_init(lv_disp_t *display) {
  esp_err_t err = esp_lv_adapter_lock(-1);
  if (err != ESP_OK) {
    return err;
  }
  err = deui_ui_init_under_lock(display);
  esp_lv_adapter_unlock();
  return err;
}

void deui_ui_update_metrics(float weight_g, float shot_time_s, float pressure_bar, float flow_ml_s,
                            bool show_shot_metrics, bool metrics_live) {
  if (esp_lv_adapter_lock(-1) != ESP_OK) {
    return;
  }

  if (!show_shot_metrics) {
    weight_g = 0.f;
    shot_time_s = 0.f;
    pressure_bar = 0.f;
    flow_ml_s = 0.f;
    metrics_live = false;
  }

  if (metrics_live != s_metrics_live) {
    s_metrics_live = metrics_live;
    apply_metric_labels(metrics_live);
    apply_metric_value_colors(metrics_live);
  }

  /** Numeric values only in labels; captions carry units (ShotSample: group pressure, group flow). */
  if (s_weight_value != NULL) {
    deui_ui_set_textf(s_weight_value, "%.1f", weight_g);
  }
  if (s_time_value != NULL) {
    deui_ui_set_textf(s_time_value, "%.1f", shot_time_s);
  }
  if (s_pressure_value != NULL) {
    deui_ui_set_textf(s_pressure_value, "%.1f", pressure_bar);
  }
  if (s_flow_value != NULL) {
    deui_ui_set_textf(s_flow_value, "%.1f", flow_ml_s);
  }

  if (deui_ui_obj_metrics_card != NULL) {
    lv_obj_invalidate(deui_ui_obj_metrics_card);
  }

  const bool show_arcs = show_shot_metrics && metrics_live;
  if (s_pressure_arc != NULL) {
    if (!show_arcs) {
      lv_arc_set_value(s_pressure_arc, 0);
      lv_obj_add_flag(s_pressure_arc, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(s_pressure_arc, LV_OBJ_FLAG_HIDDEN);
      const int pressure = (int)(pressure_bar * 100.0f + 0.5f);
      deui_ui_set_arc_marker(s_pressure_arc, pressure, k_pressure_arc_max);
    }
  }
  if (s_flow_arc != NULL) {
    if (!show_arcs) {
      lv_arc_set_value(s_flow_arc, 0);
      lv_obj_add_flag(s_flow_arc, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(s_flow_arc, LV_OBJ_FLAG_HIDDEN);
      const int flow = (int)(flow_ml_s * 100.0f + 0.5f);
      deui_ui_set_arc_marker(s_flow_arc, flow, k_flow_arc_max);
    }
  }

  esp_lv_adapter_unlock();
}

void deui_ui_update_status(const deui_ui_status_t *status) {
  if (status == NULL) {
    return;
  }

  if (esp_lv_adapter_lock(-1) != ESP_OK) {
    return;
  }

  s_ble_icon_pulse = !status->ble_connected;
  s_wifi_icon_pulse = !status->wifi_connected;
  s_scale_icon_pulse = !status->scale_connected && status->scale_scanning;

  deui_ui_status_icon_apply(s_ble_icon, s_ble_icon_pulse, s_color_primary_text, s_color_subtle_fg,
                            s_link_pulse_phase);
  deui_ui_status_icon_apply(s_wifi_icon, s_wifi_icon_pulse, s_color_primary_text, s_color_subtle_fg,
                            s_link_pulse_phase);
  if (status->scale_connected) {
    deui_ui_status_icon_apply(s_scale_icon, false, s_color_primary_text, s_color_subtle_fg, s_link_pulse_phase);
  } else if (status->scale_scanning) {
    deui_ui_status_icon_apply(s_scale_icon, true, s_color_primary_text, s_color_subtle_fg, s_link_pulse_phase);
  } else if (s_scale_icon != NULL) {
    lv_obj_set_style_text_color(s_scale_icon, lv_color_hex(s_color_subtle_fg), LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_scale_icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_color_filter_opa(s_scale_icon, LV_OPA_TRANSP, LV_PART_MAIN);
  }

  if (s_footer != NULL) {
    /** Peer name / address line omitted — BLE status is the icons only. */
    lv_obj_add_flag(s_footer, LV_OBJ_FLAG_HIDDEN);
  }

  if (s_usb != NULL) {
    if (status->power.usb_connected) {
      lv_obj_clear_flag(s_usb, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(s_usb, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (s_battery != NULL) {
    if (!status->power.available) {
      lv_obj_add_flag(s_battery, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(s_battery, LV_OBJ_FLAG_HIDDEN);
      if (status->power.charging) {
        deui_ui_label_set_static_if_changed(s_battery, "+CHG");
      } else if (status->power.low) {
        deui_ui_label_set_static_if_changed(s_battery, "!BAT");
      } else {
        deui_ui_label_set_static_if_changed(s_battery, "BAT");
      }
    }
  }

  /** One mode at a time: visibility is owned by `deui_ui_screen_*.c`. */
#if DEUI_UI_TEMP_ALWAYS_BREWING_WHEN_CONNECTED
  if (!status->ble_connected) {
    deui_ui_screen_apply_searching();
  } else {
    deui_ui_screen_apply_brewing();
  }
  const bool shot_layout = status->ble_connected;
#else
  const bool de1_live_extract =
      status->ble_connected && (status->de1_major_state == DE1_MAJOR_STATE_ESPRESSO) &&
      deui_shot_metrics_minor_is_active_extraction(status->de1_minor_state);

  if (!status->ble_connected) {
    deui_ui_screen_apply_searching();
  } else if (de1_live_extract) {
    deui_ui_screen_apply_brewing();
  } else {
    deui_ui_screen_apply_idle(status->machine_state_center, status->show_saved_shot_metrics);
  }

  const bool shot_layout = de1_live_extract;
#endif

  if (shot_layout != s_metrics_shot_layout) {
    s_metrics_shot_layout = shot_layout;
    deui_theme_palette_t theme;
    deui_theme_palette_for_mode(s_theme_mode, &theme);
    sync_metrics_card_chrome(&theme);
  }
  if (s_flow_arc != NULL) {
    if (shot_layout) {
      lv_obj_clear_flag(s_flow_arc, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(s_flow_arc, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (s_pressure_arc != NULL) {
    if (shot_layout) {
      lv_obj_clear_flag(s_pressure_arc, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(s_pressure_arc, LV_OBJ_FLAG_HIDDEN);
    }
  }

  /*
   * Brewing: keep rings above the metrics card so arc edges are fully visible (not clipped by card).
   * Non-brewing: push them behind the card/headline chrome.
   */
  if (shot_layout) {
    if (s_pressure_arc != NULL) {
      lv_obj_move_foreground(s_pressure_arc);
    }
    if (s_flow_arc != NULL) {
      lv_obj_move_foreground(s_flow_arc);
    }
  } else {
    if (s_flow_arc != NULL) {
      lv_obj_move_background(s_flow_arc);
    }
    if (s_pressure_arc != NULL) {
      lv_obj_move_background(s_pressure_arc);
    }
  }
  const bool metrics_visible = status->ble_connected;
  if (metrics_visible && deui_ui_obj_metrics_card != NULL) {
    lv_obj_move_foreground(deui_ui_obj_metrics_card);
  }
  if (!shot_layout && status->ble_connected && deui_ui_obj_machine_state != NULL) {
    lv_obj_move_foreground(deui_ui_obj_machine_state);
  }

  esp_lv_adapter_unlock();
}

void deui_ui_indicate_ring_step(int delta) {
  /*
   * Rotary direction toggles DEUI app palette: forward → light, back → dark.
   */
  if (esp_lv_adapter_lock(-1) != ESP_OK) {
    return;
  }

  deui_ui_theme_nudge_locked(delta);

  esp_lv_adapter_unlock();
}

void deui_ui_tick(void) {
  int64_t now = esp_timer_get_time();
  if (s_link_next_pulse_us == 0) {
    s_link_next_pulse_us = now;
  }
  if (now - s_link_next_pulse_us >= 340000) {
    s_link_next_pulse_us = now;
    s_link_pulse_phase = !s_link_pulse_phase;
    if (esp_lv_adapter_lock(-1) == ESP_OK) {
      if (s_ble_icon_pulse) {
        deui_ui_status_icon_apply(s_ble_icon, true, s_color_primary_text, s_color_subtle_fg,
                                  s_link_pulse_phase);
      }
      if (s_wifi_icon_pulse) {
        deui_ui_status_icon_apply(s_wifi_icon, true, s_color_primary_text, s_color_subtle_fg,
                                  s_link_pulse_phase);
      }
      if (s_scale_icon_pulse) {
        deui_ui_status_icon_apply(s_scale_icon, true, s_color_primary_text, s_color_subtle_fg,
                                  s_link_pulse_phase);
      }
      esp_lv_adapter_unlock();
    }
  }
}
