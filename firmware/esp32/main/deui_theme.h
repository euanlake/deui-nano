#pragma once

#include <stdint.h>

/**
 * Palette derived from DEUI mobile app (tailwind.config.js + Theme.Light/Dark usage).
 * Hex values are 0xRRGGBB for lv_color_hex().
 */
typedef struct {
  uint32_t screen_bg;
  uint32_t card_bg;
  uint32_t arc_track;
  /** Body / headline text: `0xebe8e8` dark, `0x1e1e1e` light (see `deui_theme_palette_for_mode`). */
  uint32_t primary_text;
  uint32_t subtle_text;
  uint32_t card_border;
  uint8_t card_border_width;
  /** Group flow ring (outer ShotSample arc); paired with `arc_track` for the unfilled segment. */
  uint32_t flow_arc;
  /** Group pressure ring (inner ShotSample arc). */
  uint32_t pressure_arc;
  uint32_t accent_ring;
  uint32_t status_bad;
  uint32_t status_good;
} deui_theme_palette_t;

typedef enum {
  DEUI_THEME_MODE_DARK = 0,
  DEUI_THEME_MODE_LIGHT = 1,
} deui_theme_mode_t;

static inline void deui_theme_palette_for_mode(deui_theme_mode_t mode, deui_theme_palette_t *out) {
  if (mode == DEUI_THEME_MODE_LIGHT) {
    /**
     * Screen + card backgrounds stay visually neutral — full-frame pink/green tints came from stray
     * LVGL defaults, not these colors. Shot arcs deliberately use navy (flow) + green (pressure).
     */
    *out = (deui_theme_palette_t){
        .screen_bg = 0xffffff,
        .card_bg = 0xffffff,
        .arc_track = 0xececec,
        .primary_text = 0x1e1e1e,
        .subtle_text = 0x757575,
        .card_border = 0xe1e1e1,
        .card_border_width = 0,
        .flow_arc = 0x002e59,
        .pressure_arc = 0x00b41c,
        .accent_ring = 0xff9900,
        .status_bad = 0xb4000b,
        .status_good = 0x00b41c,
    };
    return;
  }
  *out = (deui_theme_palette_t){
      .screen_bg = 0x000000,
      .card_bg = 0x000000,
      .arc_track = 0x171717,
      .primary_text = 0xebe8e8,
      .subtle_text = 0x757575,
      .card_border = 0x000000,
      .card_border_width = 0,
      .flow_arc = 0x002e59,
      .pressure_arc = 0x00b41c,
      .accent_ring = 0xff9900,
      .status_bad = 0xb4000b,
      .status_good = 0x00b41c,
  };
}
