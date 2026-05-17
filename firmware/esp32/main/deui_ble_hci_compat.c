/**
 * DE1 (nRF52) rejects HCI LE Set Data Length (OGF 0x08 / OCF 0x0022) with
 * BLE_ERR_UNSUPP_REM_FEATURE. NimBLE calls ble_hs_hci_util_set_data_len() on
 * every connect; skipping it is safe (default 27-byte ATT payloads still work).
 */
#include <stdint.h>

__attribute__((used))
int __wrap_ble_hs_hci_util_set_data_len(uint16_t conn_handle, uint16_t tx_octets,
                                        uint16_t tx_time) {
  (void)conn_handle;
  (void)tx_octets;
  (void)tx_time;
  return 0;
}
