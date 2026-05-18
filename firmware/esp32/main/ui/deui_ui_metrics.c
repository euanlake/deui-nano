#include "deui_ui_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum {
  k_arc_marker_half_width = 25,
  k_arc_revolution = 1200,
  k_arc_marker_fill = 100,
};

static int wrap_arc_revolution(int value) {
  int wrapped = value % k_arc_revolution;
  if (wrapped < 0) {
    wrapped += k_arc_revolution;
  }
  return wrapped;
}

static uint16_t value_to_clock_angle(int value_cent) {
  const int v = wrap_arc_revolution(value_cent);
  int angle = 270 + (v * 360) / k_arc_revolution;
  angle %= 360;
  if (angle < 0) {
    angle += 360;
  }
  return (uint16_t)angle;
}

void deui_ui_set_arc_marker(lv_obj_t *arc, int value_cent, int max_cent) {
  if (arc == NULL || max_cent <= 0) {
    return;
  }

  int clamped = value_cent;
  if (clamped < 0) {
    clamped = 0;
  }
  if (clamped > max_cent) {
    clamped = max_cent;
  }

  const int scaled = (clamped * k_arc_revolution) / max_cent;
  const int start_v = wrap_arc_revolution(scaled - k_arc_marker_half_width);
  const int end_v = wrap_arc_revolution(scaled + k_arc_marker_half_width);
  const uint16_t start_ang = value_to_clock_angle(start_v);
  const uint16_t end_ang = value_to_clock_angle(end_v);
  lv_arc_set_bg_angles(arc, start_ang, (end_ang >= start_ang) ? end_ang : (uint16_t)(end_ang + 360));
  if (lv_arc_get_value(arc) != k_arc_marker_fill) {
    lv_arc_set_value(arc, k_arc_marker_fill);
  }
}

void deui_ui_set_textf(lv_obj_t *label, const char *fmt, ...) {
  if (label == NULL || fmt == NULL) {
    return;
  }

  char buf[64];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  if (strcmp(lv_label_get_text(label), buf) == 0) {
    return;
  }
  lv_label_set_text(label, buf);
}
