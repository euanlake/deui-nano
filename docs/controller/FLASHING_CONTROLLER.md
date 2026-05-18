# Flashing DEUI Controller Firmware

This guide covers flashing `firmware/esp32` onto the DEUI ST77916/CST816 controller.

## Prerequisites

- ESP-IDF 5.5 or newer
- Python 3.10+
- USB data cable
- controller connected over USB

## macOS / Linux

Install ESP-IDF once:

```bash
git clone -b release/v5.5 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf
./install.sh esp32s3
```

Open a shell for flashing:

```bash
source ~/esp/esp-idf/export.sh
cd firmware/esp32
./dev.sh full
```

`full` performs target selection, build, full flash, and monitor.

For regular app updates:

```bash
source ~/esp/esp-idf/export.sh
cd firmware/esp32
./dev.sh quick
```

If port detection fails:

```bash
./dev.sh ports
ESPPORT=/dev/cu.usbmodemXXXX ./dev.sh quick
```

Exit monitor with `Ctrl+]`.

## Windows

Use Espressif's official ESP-IDF installer and run:

```bash
idf.py set-target esp32s3
idf.py build flash monitor
```

## Notes

- If `partitions.csv` changes, run a full flash before using `app-flash`.
- Firmware target is ST77916 QSPI + CST816 only.

## Waveshare ESP32-S3-Knob-Touch-LCD-1.8

This repo’s `board_config.h` matches the [Waveshare Knob 1.8](https://www.waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8): **ESP32-S3R8**, **16MB flash**, **8MB PSRAM**, Type‑C UART to **S3 or ESP32** depending on cable orientation.

- Flash the **ESP32-S3** image on the **ESP32-S3** serial port (`idf.py -p PORT flash` or `./dev.sh ports`). If flashing fails, flip the Type‑C plug or follow the wiki “dual MCU” instructions.
- Display/touch pins are defined in `firmware/esp32/main/board/board_config.h` and match Waveshare (QSPI **13–18**, RST **21**, BL **47**, CST816 I2C **11/12**).
- Rotary encoder on the S3 side is **GPIO8 / GPIO7** (not GPIO1/2).
- Battery sense is **GPIO1** with a **2×** divider scale in firmware — calibrate against a meter if your pack differs.

## Troubleshooting

### `Could not exclusively lock port` / `[Errno 35]` (macOS)

The **build succeeded**; **esptool** could not open the USB serial device because something else already has it open.

1. Stop **IDF serial monitor** (`Ctrl+]` in the monitor terminal), close any other terminal running `idf.py monitor`, and quit any Serial Monitor attached to that port.
2. In VS Code / Cursor, disconnect the **Espressif** or built-in serial monitor from that port.
3. Flash again. If it still fails, unplug and replug USB, then run `./dev.sh ports` to confirm the port name.
4. To see what is using the port: `lsof | grep cu.usbmodem` (quit that PID if safe).

This is unrelated to display pinout; fixing the port lock is enough for flash to succeed.