# DEUI Controller Firmware (ESP-IDF)

ESP-IDF firmware for the DEUI round ESP32-S3 controller target:

- JC3636K718-style board
- ST77916 QSPI display (`360x360`)
- CST816 touch
- outer rotary ring
- WS2812 LED ring
- DRV2605L haptics

## Scope in this repository

- DEUI-branded local setup AP and captive portal
- HTTPS OTA updates (dual `ota_0` / `ota_1` slots, GitHub Releases manifest)
- no cloud account onboarding in v1
- DE1 BLE integration surface (full transport port is in progress)
- LVGL UI with status bar and ring-step indicator

## Build and flash

Requirements:

- ESP-IDF 5.5+
- Python 3.10+
- USB data cable

```bash
cd firmware/esp32
source ~/esp/esp-idf/export.sh
./dev.sh full
```

For iterative app updates (after OTA partition migration):

```bash
cd firmware/esp32
source ~/esp/esp-idf/export.sh
./dev.sh quick
```

If partitions change (`partitions.csv`), run `full` again.

## Project naming

- CMake project: `deui_nano` (`project(deui_nano VERSION x.y.z)`)
- Build artifact: `build/deui_nano.bin`
- Release asset: `deui_nano-esp32s3-vX.Y.Z.bin`

## OTA

- Manifest: `ota/manifest.json` (CI-updated on tagged releases)
- Kconfig: **DEUI OTA** → `CONFIG_DEUI_OTA_ENABLE`, `CONFIG_DEUI_OTA_MANIFEST_URL`
- Portal: **Software Update** at `/updates`
- Docs: `docs/controller/OTA.md`

## Browser install (DIY)

`https://euanlake.github.io/deui-nano-testing/`

## Board pins

QSPI display, backlight, and CST816 I2C lines in `main/board/board_config.h` follow Waveshare reference wiring (PCLK 13, CS 14, data 15–18, RST 21, backlight 47, touch SDA 11 / SCL 12). Touch reset/interrupt pins default to `GPIO_NUM_NC`—set them in `board/board_config.h` if `esp_lcd_touch_cst816s` init fails on your PCB.

## Layout

- `main/board/` board drivers and pin mapping (`board_config.h`)
- `main/input/` physical ring polling
- `main/net/` Wi-Fi setup AP + captive portal + OTA (`main/net/ota/`)
- `main/ble/` DE1 BLE integration entrypoint and scale/weight-stop support
- `main/brew/` shot metrics computation
- `main/power/` power policy
- `main/ui/` LVGL UI core, plus per-mode screens under `main/ui/screens/`

See `docs/controller/FLASHING_CONTROLLER.md`, `docs/controller/SETUP_GUIDE.md`, and `docs/controller/OTA.md` for operator flow.
