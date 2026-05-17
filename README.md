# DEUI Nano Testing Workspace

This repository contains firmware and supporting documentation for a DEUI-branded round ESP32-S3 controller.

Primary target:

- ESP32-S3 controller board (JC3636K718-class)
- ST77916 QSPI round display (`360x360`)
- CST816 touch
- rotary ring input
- WS2812 LED ring
- DRV2605L haptics

## What this app does

Current firmware scope (v1) is centered on local controller operation:

- local setup AP and captive portal for Wi-Fi onboarding
- DE1-focused BLE integration path
- LVGL-based UI with status + live metric rendering
- hardware validation feedback for ring, LEDs, and haptics

Out of scope in v1:

- cloud account onboarding
- portal-authenticated setup flow

## Repository layout

- `firmware/esp32/`
  - canonical ESP-IDF firmware project (`deui_controller`)
- `docs/`
  - operator and protocol docs
- `docs/CODEBASE_GUIDE.md`
  - architecture and source-map guide for senior engineering reviews
- `fonts/`
  - embedded font source files and related font assets
- `lib/lvgl/`
  - vendored LVGL source tree
- `main/`
  - Arduino-style parallel/prototype code path (not the canonical ESP-IDF runtime)

Generated/vendor-heavy directories under firmware:

- `firmware/esp32/build/`
- `firmware/esp32/managed_components/`

## Quick start (ESP-IDF)

From repo root:

```bash
cd firmware/esp32
source ~/esp/esp-idf/export.sh
./dev.sh full
```

For iterative updates:

```bash
cd firmware/esp32
source ~/esp/esp-idf/export.sh
./dev.sh quick
```

If USB port auto-detection fails:

```bash
./dev.sh ports
ESPPORT=/dev/cu.usbmodemXXXX ./dev.sh quick
```

## Recommended docs

- Firmware target readme: `firmware/esp32/README.md`
- Code structure and review map: `docs/CODEBASE_GUIDE.md`
- Flashing workflow: `docs/controller/FLASHING_CONTROLLER.md`
- Setup/onboarding flow: `docs/controller/SETUP_GUIDE.md`
- DE1 BLE protocol context: `docs/de1-bluetooth-protocol.md`

## Attribution and lineage

The project includes upstream-derived material and DEUI-specific adaptations. See:

- `THIRD_PARTY_NOTICES.md`
