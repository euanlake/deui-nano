#include "deui_wifi_internal.h"

uint32_t deui_wifi_reconnect_delay_ms(uint8_t disconnect_count) {
  uint32_t delay_ms = 1000;
  uint8_t i;

  for (i = 1; i < disconnect_count; ++i) {
    if (delay_ms >= 30000) {
      return 30000;
    }
    delay_ms *= 2;
  }
  if (delay_ms > 30000) {
    delay_ms = 30000;
  }
  return delay_ms;
}

bool deui_wifi_reopen_ap_after_disconnects(uint8_t disconnect_count) {
  return disconnect_count >= 4;
}
