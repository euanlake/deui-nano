#include "deui_ble_internal.h"

#include <stdio.h>

void deui_ble_major_state_name_for_log(uint8_t v, char *out, size_t cap) {
  const char *n = NULL;
  switch (v) {
  case 0x00: n = "Sleep"; break;
  case 0x01: n = "GoingToSleep"; break;
  case 0x02: n = "Idle"; break;
  case 0x03: n = "Busy"; break;
  case 0x04: n = "Espresso"; break;
  case 0x05: n = "Steam"; break;
  case 0x06: n = "HotWater"; break;
  case 0x07: n = "ShortCal"; break;
  case 0x08: n = "SelfTest"; break;
  case 0x09: n = "LongCal"; break;
  case 0x0a: n = "Descale"; break;
  case 0x0b: n = "FatalError"; break;
  case 0x0c: n = "Init"; break;
  case 0x0d: n = "NoRequest"; break;
  case 0x0e: n = "SkipToNext"; break;
  case 0x0f: n = "HotWaterRinse"; break;
  case 0x10: n = "SteamRinse"; break;
  case 0x11: n = "Refill"; break;
  case 0x12: n = "Clean"; break;
  case 0x13: n = "InBootLoader"; break;
  case 0x14: n = "AirPurge"; break;
  case 0x15: n = "ScheduledWake"; break;
  default: break;
  }
  if (n != NULL) {
    snprintf(out, cap, "%s", n);
  } else {
    snprintf(out, cap, "unknown(0x%02x)", v);
  }
}

void deui_ble_minor_state_name_for_log(uint8_t v, char *out, size_t cap) {
  const char *n = NULL;
  switch (v) {
  case 0x00: n = "NoState"; break;
  case 0x01: n = "HeatWaterTank"; break;
  case 0x02: n = "HeatWaterHeater"; break;
  case 0x03: n = "StabilizeMixTemp"; break;
  case 0x04: n = "PreInfuse"; break;
  case 0x05: n = "Pour"; break;
  case 0x06: n = "Flush"; break;
  case 0x07: n = "Steaming"; break;
  case 0x08: n = "DescaleInit"; break;
  case 0x09: n = "DescaleFillGroup"; break;
  case 0x0a: n = "DescaleReturn"; break;
  case 0x0b: n = "DescaleGroup"; break;
  case 0x0c: n = "DescaleSteam"; break;
  case 0x0d: n = "CleanInit"; break;
  case 0x0e: n = "CleanFillGroup"; break;
  case 0x0f: n = "CleanSoak"; break;
  case 0x10: n = "CleanGroup"; break;
  case 0x11: n = "PausedRefill"; break;
  case 0x12: n = "PausedSteam"; break;
  case 0x13: n = "UserNotPresent"; break;
  case 0x14: n = "SteamPuff"; break;
  case 0xc8: n = "Error_NaN"; break;
  case 0xc9: n = "Error_Inf"; break;
  case 0xca: n = "Error_Generic"; break;
  case 0xcb: n = "Error_ACC"; break;
  case 0xcc: n = "Error_TSensor"; break;
  case 0xcd: n = "Error_PSensor"; break;
  case 0xce: n = "Error_WLevel"; break;
  case 0xcf: n = "Error_DIP"; break;
  case 0xd0: n = "Error_Assertion"; break;
  case 0xd1: n = "Error_Unsafe"; break;
  case 0xd2: n = "Error_InvalidParm"; break;
  case 0xd3: n = "Error_Flash"; break;
  case 0xd4: n = "Error_OOM"; break;
  case 0xd5: n = "Error_Deadline"; break;
  case 0xd6: n = "Error_HiCurrent"; break;
  case 0xd7: n = "Error_LoCurrent"; break;
  case 0xd8: n = "Error_BootFill"; break;
  default: break;
  }
  if (n != NULL) {
    snprintf(out, cap, "%s", n);
  } else {
    snprintf(out, cap, "unknown(0x%02x)", v);
  }
}

const char *deui_ble_machine_state_label(uint8_t major, uint8_t minor) {
  switch (major) {
  case 0x00:
  case 0x01:
    return "Sleep";
  case 0x02:
    if (minor == 0x00) {
      return "Ready";
    }
    if (minor == 0x01 || minor == 0x02 || minor == 0x03) {
      return "Heating";
    }
    return "Idle";
  case 0x03:
    return "Busy";
  case 0x04:
    return "Espresso";
  case 0x05:
    return "Steam";
  case 0x06:
    return "Hot water";
  case 0x07:
  case 0x09:
    return "Calibrate";
  case 0x08:
    return "Self test";
  case 0x0a:
    return "Descale";
  case 0x0b:
    return "Error";
  case 0x0c:
    return "Init";
  case 0x0f:
    return "Rinse";
  case 0x10:
    return "Steam rinse";
  case 0x11:
    return "Refill";
  case 0x12:
    return "Clean";
  case 0x13:
    return "Bootloader";
  case 0x14:
    return "Air purge";
  case 0x15:
    return "Wake";
  default:
    return "Idle";
  }
}

bool deui_ble_is_espresso_major(uint8_t major) {
  return major == 0x04;
}
