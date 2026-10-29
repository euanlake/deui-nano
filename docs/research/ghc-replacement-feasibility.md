# GHC replacement: feasibility research

October 2026. Research into replacing the DE1's group head controller (GHC) with a DEUI-style ESP32 round-display controller, or reaching the same result another way.

## Bottom line

It's feasible, and probably easier than it looks, but the protocol work isn't the hard part. The DE1 has two separate control channels, and only one of them is a mystery.

1. **The tablet channel is already documented.** The protocol the GHC is assumed to speak ("serial to the firmware") is actually the tablet channel. It's a plain-text UART protocol, and it's the same data DEUI already decodes over BLE. Decent now exposes it on a USB-C adaptor.
2. **The GHC channel is separate and undocumented.** It has its own 6-pin port on the DC board, added in v1.3. The DE1 detects a "touch controller" and an "LED controller" separately. A photo of the GHC board (Rev F) shows a QFN chip beside a crystal, which suggests a small microcontroller is on board too. "Emulating the GHC" means reproducing whatever that board says on the 6-pin cable, which is a logic-analyser job.
3. **UL compliance decides which channel you need.** On a machine with an active GHC, the firmware won't let the tablet channel *start* espresso, steam, hot water or flush. Stop still works. Only the GHC port (or removing the GHC) gets around that.

The recommended order:

1. **Now, no new hardware:** ship the "arm then tap" flow in DEUI over BLE.
2. **This week:** ask Decent the specific questions listed below.
3. **In parallel:** spend a day on hardware recon (chip markings plus a logic analyser on the GHC cable). That tells you within a day whether GHC emulation is a weekend project or a slog.
4. **Then decide on the hybrid build:** GHC-port emulation for local start/stop, plus the existing BLE (or wired serial) link for telemetry and profiles.

## How the DE1 is wired for control

```
                        ┌──────────────────────────── DE1 main CPU (DC board) ─────────────────────────────┐
                        │                                                                                   │
   TABLET CHANNEL       │  6-pin header (back panel)            GHC CHANNEL       GHC port (added v1.3)     │
   ──────────────       │  UART 115200 8N1, RTS/CTS + P05 busy  ───────────       6-pin cable to group head │
                        └───────┬───────────────────────────────────────────────────────┬───────────────────┘
                                │                                                       │
              ┌─────────────────┼──────────────────┐                      ┌─────────────┴─────────────┐
              │                 │                  │                      │  GHC PCB                  │
        Decent BLE module   Decent USB-C     community boards            │  • touch controller chip  │
        (nRF51822)          adaptor (beta)   (Plucky/DAYBREAK,            │  • LED controller chip    │
              │             CDC-ACM serial   ESP32, 2020)                 │  (bus: unconfirmed,       │
         BLE GATT A001…A013     │                                         │   likely I2C)             │
              │                 │                                         └───────────────────────────┘
        tablet / DEUI       computer / tablet
```

### Tablet channel (documented)

Decent's BLE module is a small board that plugs into a 6-pin header behind the back panel. It bridges BLE to the main CPU over UART. The UART protocol mirrors the BLE GATT layout one-to-one. Each characteristic A001–A013 gets a letter A–S.

| Direction | Frame | Meaning |
|---|---|---|
| out | `<B>02` | write hex payload to characteristic B (RequestedState = Idle) |
| out | `<+M>` | subscribe to characteristic M (ShotSample) |
| out | `<-M>` | unsubscribe |
| in | `[M]hexhex…` | notification or reply for characteristic M |

The link is 115200 8N1, with newline-terminated lines and no checksum. The BLE module uses RTS/CTS flow control, plus a "P05" busy line added around July 2020 because the nRF51822's UART drops bytes. The payload bytes are exactly what DEUI already parses in `firmware/esp32/main/ble/deui_ble_parse.c`, so a wired transport would reuse all the decode logic.

Decent's **USB-C adaptor** (beta, February 2026) sits in that same socket, with the BLE module plugged on top of it. Only one of BLE or USB-C can be active at a time. Both de1app (`de1plus/usb/usb.tcl`, `de1_usb.tcl`) and Decent's newer app decaid support it.

### GHC channel (undocumented)

What's known:

- The GHC needed new communication ports on the DC board, which is why v1.0 machines can't take one. Decent's parts list shows a 6-pin controller cable.
- MMR register `0x80381C` (GHC info) is a bitmask:
  - `0x1` = LED controller present
  - `0x2` = touch controller present
  - `0x4` = GHC active
  - `0x80000000` = factory mode

  (Sources: decaid's `de1.models.dart` and pyDE1's `c_api.py`.)
- GHC behaviour fixes have shipped as DE1 firmware builds, for example build 1328 "fixes GHC tap-tap for small changes". The touch logic lives in the DE1's main firmware.
- The LED ring shows live flow (blue) and pressure (green) as moving dots. Dragging a finger sets a new value, and taps near the current value nudge it by ±0.1.

What that implies: the main CPU detects two functions independently over a shared bus, possibly I2C, and much of the touch logic lives in DE1 firmware.

**Update (from the Brewing with Euan front-panel guide photos):** the GHC PCB is a ~34 mm round board (Rev F) on four aluminium standoffs. It has:

- a 6-pin connector (J13)
- U1, a QFN chip, sitting next to X1, a crystal
- U2, another QFN
- U3, a TSSOP chip, with three transistors and low-ohm resistors that look like LED drive
- a separate yellow wire screwed to a standoff (probably earth)

The crystal suggests U1 is a microcontroller, so the GHC likely does run its own firmware. Read the chip markings to confirm.

One more point: **flow and pressure numbers don't come from the GHC.** The DE1 only paints them onto the ring. A display-based replacement still needs the tablet channel (BLE or serial) for real numbers.

## The UL start interlock

Machines that ship with a GHC are UL-listed on the basis that dangerous operations start at the group head.

- **Active GHC (bit `0x4` set):** a remote start request doesn't start anything. It arms the ready state (the group head glows white), and the user taps the GHC to go. Remote **stop** still works. Decent's decaid notes say plainly not to bypass this or present remote controls as able to start.
- **GHC present but untouched (bits 1/2 only):** the tablet can start until the GHC is first touched. After that it goes active.
- **No GHC (value 0):** remote starts work. About 500 early machines shipped like this. This matches your experience that unplugging the GHC unlocks wireless control. It's worth confirming that `0x80381C` reads `0` after a full power cycle with the GHC disconnected, in case the firmware latches "active".
- Plucky's source includes an undocumented factory command that turns on remote control with the GHC fitted. Its author flags it as unsupported by Decent and a possible safety hazard. Don't build on it, especially for anything other people will use.

## Options compared

| | A. Wireless (BLE, today's DEUI) | B. Wired tablet channel (USB-C adaptor or 6-pin UART) | C. GHC emulation (ESP32 on GHC port) |
|---|---|---|---|
| Protocol known? | Yes | Yes (same as BLE) | No, needs reverse engineering |
| Telemetry (flow, pressure, temps) | Yes | Yes | Coarse at best (from LED ring writes) |
| Start espresso/steam/water on a GHC machine | Arm only, user taps GHC | Arm only, same rule | Yes, as a local touch |
| Start with GHC removed | Yes | Yes | n/a |
| Hardware work | None | Cable, level shifting, power; the adaptor needs USB host on the ESP32-S3 | PCB, connector, power, heat, moisture |
| Fragility | Low | Low | Medium: depends on Decent's GHC firmware handling and chip choices |
| Can ship to others | Yes | Yes | Only with Decent's blessing (it's the UL control) |

### Recommendation

**Short term (A, arm then tap).** Keep BLE, and add an "arm" action. DEUI selects espresso, steam or water and sends the start request. The machine glows white, and the user taps the GHC. This needs no hardware, it's compliant, and it works on every machine. For your own bench testing, unplugging the GHC gives full remote control.

**Product direction (hybrid C + A).** If the goal is a round-display controller that sits where the GHC sits:

- The GHC port carries local start/stop. Only a physical touch on the device itself triggers a start, which keeps the UL intent intact.
- BLE carries telemetry, profiles and settings, reusing DEUI's existing code.
- Don't let a Wi-Fi or app command "press" the emulated GHC. That rebuilds the bypass the interlock exists to prevent.

**Wired serial (B)** is worth it only if BLE reliability is a real problem. It doesn't change what you're allowed to start. The USB-C adaptor turns BLE off while it's in use, so the tablet would lose its link unless the ESP32 multiplexes, as Plucky did.

## Could REA help decode the GHC firmware?

Mostly no, for two reasons.

1. **The GHC's own firmware isn't reachable from software.** The board probably has a small MCU (see the update above), but its firmware never passes through anything REA can open. Getting it means reading the chip directly, which is usually read-protected. Capturing the wire protocol is easier and is what emulation needs.
2. **The firmware that does contain the GHC logic is encrypted.** The DE1 images that ship in de1app (`de1plus/fw/bootfwupdate.dat`, about 464 KB, and `benglefw.dat`, about 711 KB) have flat 8.0 bits/byte entropy and no readable strings. They're decrypted on the machine during the update. REA's firmware tooling (Binwalk/Unblob, then Ghidra/IDA) would only see noise. Getting plaintext would mean dumping the main CPU over a debug port. That's probably read-protected, and it's beyond what you need for interoperability.

Where REA (github.com/morluto/rea) can help is the step that matters: it ingests **sigrok logic-analyser captures with protocol-decoder annotations**. Once you have recordings of the GHC cable, an agent with REA, or plain Python over PulseView exports, can help diff captures and build the register map. It's an analysis aid on top of the logic analyser, not a replacement for it.

Only install it from github.com/morluto/rea, rea.tools or the `rea-agents` npm package. There are trojanised copycat repos.

## Hardware reverse-engineering plan

**Safety first.** The DE1 has mains-powered heaters and pumps inside. Open it only when it's unplugged. Do the live probing at the GHC end of the 6-pin cable, which is low voltage, never on the DC board.

**Kit (about £20–£150):**

- A logic analyser with at least 8 channels. A Saleae is nice; a £15 FX2 clone with sigrok/PulseView works fine for I2C and UART.
- A multimeter.
- Optionally a cheap oscilloscope.
- A spare GHC cable from Decent's parts store, to build a pass-through tap harness. That's better than back-probing the original.

**Steps:**

1. **Identify the chips.** Remove the GHC (Decent's v1.1 upgrade guide shows the access, with a Torx T10 driver), photograph the PCB and read the IC markings. If the touch and LED chips are off the shelf, their datasheets give you most of the register map before you capture anything.
2. **Map the pins.** With the machine on and the GHC connected, measure each pin against ground. Look for 3.3 V or 5 V supply, ground, idle-high lines with pull-ups (I2C SDA/SCL, or UART TX), and any interrupt line. Note the supply current budget too. A 360×360 display plus an ESP32-S3 radio can peak at several hundred mA, and the port was designed for two small chips.
3. **Capture, labelled.** Record:
   - power-on (the chip-detection sequence)
   - idle
   - each button tap, one at a time
   - press and hold
   - drag on the ring
   - a full espresso (LED ring updates)
   - flush, steam and hot water

   Keep a written log of what you did and when, so captures can be lined up against actions.
4. **Decode and diff.** Use PulseView's I2C or UART decoders. Diff the captures to find which register reads change on a touch and how ring position is encoded. Map the LED writes to machine states (white = armed, magenta pulse = interrupted profile upload, and so on).
5. **Sniff passively with an ESP32.** Put an ESP32 on the tap harness as a read-only listener and decode live on its screen. This carries no risk and proves you understand the protocol.
6. **Go in-line (MITM).** Pass the real GHC through the ESP32 and inject synthetic touches. The real GHC stays as a fallback.
7. **Emulate fully.** The ESP32 answers as both chips (the ESP32-S3 supports I2C target mode), with the real GHC removed. Check that `0x80381C` reads like a real GHC (`0x7` after first touch), and test every operation, including stop mid-shot.

If step 1 shows a microcontroller rather than plain touch and LED chips, the plan still holds. You'd still capture the wire protocol rather than try to dump that chip.

## Questions to take to Decent

1. What's the GHC port's pinout, voltage, bus type and power budget? What are the touch and LED controller part numbers, and their addresses if it's I2C?
2. Is there, or will there be, a sanctioned way for a third-party group-head device to act as the UL-compliant local start? decaid's Bengle design notes mention future-proofing for a "GHC replacement / 3rd-party peripherals", so this may already be on their roadmap.
3. How stable is GHC handling across firmware builds and GHC hardware revisions? (The parts list shows several GHC versions, for example black v1.4.)
4. What's their position on a machine running with the GHC removed, for UL and warranty, if DEUI recommends it?
5. What temperature and humidity does the GHC mount see at the group head? The original GHC had ghost touches from moisture, which they fixed with a metal plate and gasket. Displays and the ESP32-S3 are typically rated to about 60–85 °C.
6. Can the USB-C adaptor coexist with BLE, or with more than one client? What's the timeline for the 4-port hub version?

## Risks and constraints

- **Heat and steam at the group head** are the real long pole for a display product, not the protocol.
- **Firmware updates** could change GHC detection or timing and break emulation without warning.
- **Certification.** Replacing the GHC changes the UL-relevant control. That's fine for a personal machine, but a product needs Decent on side.
- **Single BLE client.** The DE1's BLE module takes one connection at a time, so a DEUI device on BLE competes with the tablet.
- **Unverified so far:** the bus type, connector part, voltage levels and power budget of the GHC port. All of these come out of steps 1–2 above.

## Sources

- de1app (Decent, GPL), `de1plus/usb/usb.tcl` and `de1plus/de1_usb.tcl` (serial framing), `de1plus/vars.tcl` (`ghc_required`, GHC states), `de1plus/de1_comms.tcl` (MMR map): https://github.com/decentespresso/de1app
- decaid (Decent), `doc/device-notes/de1.md` (GHC start restriction, serial notes), `lib/src/models/device/impl/de1/de1.models.dart` (GHC bitmask), Bengle foundation design doc: https://github.com/decentespresso/decaid
- pyDE1 (Jeff Kletsky), `MMRGHCInfoBitMask`, firmware build notes: https://github.com/jeffsf/pyDE1
- Plucky (Reed Taylor, ESP32 bridge), `include/config.hpp`, `INSTALLATION.md` (6-pin socket, flow control, P05): https://github.com/reedtaylor/plucky
- DESIRE (Reed Taylor): https://github.com/reedtaylor/desire
- REA, Reverse Engineer Anything (morluto): https://github.com/morluto/rea
- Decent: USB-C adaptor beta post and docs; v1.3 hardware post; GHC upgrade guide for v1.1; parts listings for the GHC and its cable. These were read via search excerpts only, because decentespresso.com was blocked from the research sandbox:
  - https://decentespresso.com/blog/now_in_beta_usbc_adaptor_for_de1
  - https://www.decentespresso.com/docs/de1_usbc_adapter_documentation
  - https://decentespresso.com/blog/whats_new_in_v13_hardware2020
  - https://decentespresso.com/docs/group_head_controller_upgrade_for_v11_machines
- The Diaspora (Basecamp) and Home-Barista forums were also blocked from the sandbox. No public Diaspora post describing the GHC protocol turned up in search.
