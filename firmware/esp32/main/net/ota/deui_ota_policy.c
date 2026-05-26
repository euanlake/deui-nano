#include "deui_ota_policy.h"

#include "deui_ble_client.h"
#include "deui_wifi.h"
#include "deui_wifi_internal.h"

bool deui_ota_has_internet(void) {
  deui_wifi_info_t info = {0};
  deui_wifi_get_info(&info);
  return info.sta_connected && info.sta_ip[0] != '\0';
}

bool deui_ota_setup_session_active(void) {
  return deui_wifi_ap_has_station() || deui_wifi_ap_awaiting_dhcp() || deui_wifi_join_boost_active() ||
         g_deui_wifi.sta_provision_active;
}

static bool deui_ota_is_brewing(void) {
  deui_ble_status_t ble = {0};
  deui_ble_get_status(&ble);
  return ble.connected && ble.de1_major_state == DE1_MAJOR_STATE_ESPRESSO;
}

bool deui_ota_policy_should_defer_auto(void) {
  return deui_ota_setup_session_active() || deui_ota_is_brewing();
}

bool deui_ota_policy_should_defer(deui_ota_trigger_t trigger) {
  if (trigger != DEUI_OTA_TRIGGER_AUTO_SLEEP) {
    return false;
  }
  return deui_ota_policy_should_defer_auto();
}

bool deui_ota_policy_can_download(deui_ota_trigger_t trigger) {
  if (deui_ota_is_brewing()) {
    return false;
  }
  if (!deui_ota_has_internet()) {
    return false;
  }
  if (trigger == DEUI_OTA_TRIGGER_AUTO_SLEEP && deui_ota_setup_session_active()) {
    return false;
  }
  return true;
}
