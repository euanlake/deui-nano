#include "deui_ble_client.h"

/** ShotSample multi-byte fields are big-endian on the wire (matches `docs/de1-bluetooth-protocol.md` + `archive/arduino-prototype/de1_ble_client.cpp`). */
static uint16_t u16_payload_be(const uint8_t *data) {
  return ((uint16_t)data[0] << 8) | (uint16_t)data[1];
}

static uint32_t u32_payload_be(const uint8_t *data) {
  return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

bool deui_ble_parse_shot_sample(const uint8_t *payload, size_t len, de1_shot_sample_t *out) {
  if (payload == NULL || out == NULL || len < 12) {
    return false;
  }

  out->sample_time = u16_payload_be(payload + 0);
  out->group_pressure = (float)u16_payload_be(payload + 2) / 4096.0f;
  out->group_flow = (float)u16_payload_be(payload + 4) / 4096.0f;
  out->mix_temperature = (float)u16_payload_be(payload + 6) / 256.0f;
  out->head_temperature = (float)u32_payload_be(payload + 8) / 65536.0f;
  return true;
}
