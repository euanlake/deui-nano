#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
  bool connected;
  bool scanning;
  bool has_weight;
  float weight_g;
  int battery_percent;
  int64_t last_update_us;
  char device_name[32];
} deui_scale_status_t;

esp_err_t deui_scale_init(void);
void deui_scale_tick(bool allow_scan);
void deui_scale_get_status(deui_scale_status_t *status);

/**
 * Parse a BOOKOO 20-byte packet.
 * Returns true when packet length/format is valid and writes parsed values.
 */
bool deui_scale_parse_bookoo_packet(const uint8_t *packet, uint16_t len, float *weight_g_out,
                                    int *battery_percent_out);
