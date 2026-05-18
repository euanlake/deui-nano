#include "deui_ble_internal.h"

#include <string.h>

#include "os/os_mbuf.h"

bool deui_ble_copy_state_bytes(const struct os_mbuf *om, uint8_t out[2]) {
  if (om == NULL || out == NULL) {
    return false;
  }

  uint16_t raw_len = OS_MBUF_PKTLEN(om);
  uint16_t n = raw_len > 2u ? 2u : raw_len;
  if (n < 2u) {
    return false;
  }
  memset(out, 0, 2);
  return os_mbuf_copydata((struct os_mbuf *)om, 0, n, out) == 0;
}
