# DEUI Controller Setup Guide

This guide documents the v1 local setup flow for the DEUI controller.

## Setup principles

- DEUI branding only
- no cloud login in v1
- no portal authentication in v1
- local setup AP with mDNS (no captive portal popup)

## First boot behavior

1. Controller starts setup AP automatically.
2. AP SSID is `DEUI-XXXX`.
3. AP password is `deui-setup`.
4. Join the AP from your phone or computer.
5. Open **http://deui.local/** in the browser (or **http://192.168.4.1/** if mDNS is unavailable).
6. Use **Wi-Fi** to save your home network; use **Stop at weight** for the shot target.

## Portal features (v1)

- Wi-Fi scan and save home SSID/password
- stop-at-weight target (grams)
- network reset (on the Wi-Fi screen)

Not included in v1:

- cloud account login
- recipes editor
- OTA portal
- diagnostics console
- hostname editing on the portal (fixed to `deui` / `deui.local`)

## Connectivity indicators

- BLE icon/state reflects DE1 BLE path status.
- Wi-Fi state reflects AP-only vs STA-connected state.
- USB indicator appears only when USB is connected.
- Battery indicator appears only when battery telemetry is available, with low/charging state.

## Ring validation behavior (v1)

Physical outer ring rotation does not control brew settings yet.
Each detent triggers a visible UI cue so hardware wiring can be verified quickly.
