# Touch Controller Debug Handoff

## Context
- Hardware target: Waveshare `ESP32-S3-Knob-Touch-LCD-1.8` (dual MCU board: ESP32-S3 + ESP32).
- Firmware under debug: `firmware/esp32` in this repo.
- Touch controller expected: CST816 on I2C (`TP_SDA`, `TP_SCL`) with nominal address `0x15`.

## Scope Completed
- Verified parser/UI issues were not the blocker for touch input.
- Drove investigation down to electrical reachability of touch IC from ESP32-S3 path.
- Added and exercised runtime diagnostics in `firmware/esp32/main/board/board_display.c`.

## Evidence Summary (Runtime)
- Repeated boot logs on ESP32-S3 firmware show:
  - Touch bus init succeeds (`err=0`).
  - I2C scan finds only `0x5A` (DRV2605 haptic).
  - Touch probes for `0x14` and `0x15` both fail consistently.
  - CST816 wake write returns `ESP_ERR_INVALID_STATE`.
  - LVGL starts without touch (`LVGL running without touch input`).
- Haptic initialization succeeds (`DRV2605L status=0xE0`) on the same bus/pins, proving bus is active.

## Hypotheses and Outcomes
- **H1: Touch is at `0x15` (or `0x14`) and detectable by probe** -> **Rejected**
  - `probe(...): addr0x14=261 addr0x15=261` across all checkpoints.
- **H2: Touch bus bring-up/configuration is wrong** -> **Rejected**
  - Bus init diagnostic reports valid config and `err=0`.
- **H3: Speed mismatch (100k/300k) is primary blocker** -> **Inconclusive / unlikely primary**
  - No ACK before touch panel-IO speed selection is even relevant.
- **H4: Global I2C state machine issue** -> **Rejected**
  - Another device on same bus (`0x5A`) is stable and functional.
- **H5: Touch not electrically reachable from S3 in current board path/state** -> **Strongly supported**
  - Persistent `0x5A`-only scans and no touch ACK.
- **H6: TP_RST handling is preventing startup** -> **Rejected**
  - Forced reset pulse diagnostic (low/high cycle on GPIO10) did not make `0x14/0x15` appear.
- **H7: USB-C orientation/path selects different MCU and changes what can be flashed** -> **Confirmed**
  - On flipped side, esptool reported chip is ESP32 (not S3), causing S3 flash to fail.

## Assumptions Checked
- Assumption: touch INT/RST pin mapping might be incorrect in firmware.
  - Action: tested both mapped and unbound behavior; no impact on touch reachability.
  - Status: **not root cause** with current evidence.
- Assumption: issue could be software-only (LVGL/input stack).
  - Action: low-level I2C probes and wake transactions instrumented before LVGL touch registration.
  - Status: **rejected**.
- Assumption: wrong USB side could invalidate flashing/testing conclusions.
  - Action: validated both USB sides; one enumerates ESP32-S3, other ESP32.
  - Status: **confirmed relevant**.

## Code Changes Made in This Repo
- `firmware/esp32/main/board/board_display.c`
  - Added `[DBG]` runtime diagnostics:
    - bus init config/result
    - GPIO snapshot for TP_INT/TP_RST
    - direct probes (`0x14`, `0x15`) at multiple stages
    - wake transaction result
    - scan summary
  - Kept touch bring-up sequence after panel `disp_on` settle delay.
  - Removed rejected forced-reset experiment (`H6`) after evidence collection.
- `firmware/esp32/main/board/board_config.h`
  - Touch INT/RST currently unbound (`GPIO_NUM_NC`) to keep path close to vendor I2C-only demo assumptions during diagnosis.

## External Validation Work
- Flashed vendor dual-bin firmware pair successfully:
  - ESP32 binary to ESP32 side.
  - ESP32-S3 binary to ESP32-S3 side.
- Flashed vendor ESP-IDF `08_LVGL_Test` successfully after fixing that example's flash-size config mismatch (8MB config with 8M app partition + system partitions).

## Key Commands Used
- Chip detection:
  - `esptool.py --chip auto -p <port> chip_id`
- Repo firmware build:
  - `source ~/esp/esp-idf/export.sh && idf.py build`
- Vendor demo flash:
  - `idf.py set-target esp32s3`
  - `idf.py -p /dev/cu.usbmodem1101 flash`

## Current Best Explanation
- The ESP32-S3 firmware path is functioning, but the CST816 is not visible on S3 I2C in the tested hardware state.
- Most probable class of cause is board-level pathing/variant/orientation/connectivity rather than application firmware logic.

## Recommended Next Steps (Senior Handoff)
1. Run untouched vendor `08_LVGL_Test` on the confirmed ESP32-S3 USB side and verify touch behavior end-to-end.
2. If vendor demo touch still fails:
   - Treat as hardware path issue (connector/FPC/board revision/mux behavior).
   - Verify with schematic-net continuity and board revision BOM notes.
3. If vendor demo touch works but repo firmware does not:
   - Diff vendor touch init path vs `board_display.c` around:
     - I2C device registration lifecycle
     - wake transaction timing
     - touch driver creation parameters
4. After final resolution, remove `[DBG]` diagnostics in `board_display.c`.

## Open Risks / Unknowns
- No definitive confirmation yet of touch behavior under vendor demo runtime after successful flash.
- Board variant differences may exist despite same product name; current evidence suggests this is plausible.
