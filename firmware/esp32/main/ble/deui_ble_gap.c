#include "deui_ble_client.h"
#include "deui_ble_internal.h"

#include <ctype.h>
#include <string.h>

bool deui_ble_name_matches_pattern(const uint8_t *nm, uint8_t len) {
  if (nm == NULL || len == 0) {
    return false;
  }

  char lower[41];
  size_t copy = len;
  if (copy >= sizeof(lower)) {
    copy = sizeof(lower) - 1;
  }

  size_t span = copy;
  for (size_t i = 0; i < copy && i < sizeof(lower) - 1; ++i) {
    unsigned char ch = nm[i];
    if (ch == '\0') {
      span = i;
      break;
    }
    lower[i] = (char)tolower((int)ch);
  }
  lower[span] = '\0';

  if (!strcmp(lower, "nrf5x")) {
    return true;
  }
  if (strstr(lower, "de1") || strstr(lower, "decent") || strstr(lower, "de1pro") ||
      strstr(lower, "de1+") || strstr(lower, "de1 ")) {
    return true;
  }
  return false;
}

static const char *const s_gap_name = "DEUI-ST77916";

const char *deui_ble_gap_name(void) {
  return s_gap_name;
}
