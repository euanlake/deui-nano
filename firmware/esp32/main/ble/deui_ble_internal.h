#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct os_mbuf;

void deui_ble_major_state_name_for_log(uint8_t v, char *out, size_t cap);
void deui_ble_minor_state_name_for_log(uint8_t v, char *out, size_t cap);
const char *deui_ble_machine_state_label(uint8_t major, uint8_t minor);
bool deui_ble_is_espresso_major(uint8_t major);

bool deui_ble_name_matches_pattern(const uint8_t *nm, uint8_t len);
bool deui_ble_copy_state_bytes(const struct os_mbuf *om, uint8_t out[2]);
