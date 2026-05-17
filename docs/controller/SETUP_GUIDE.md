# DEUI Controller Setup Guide

This guide documents the v1 local setup flow for the DEUI controller.

## Setup principles

- DEUI branding only
- no cloud login in v1
- no portal authentication in v1
- local captive portal for Wi-Fi onboarding

## First boot behavior

1. Controller starts setup AP automatically.
2. AP SSID is `DEUI-XXXX`.
3. On-device setup page shows AP credentials and portal URL.
4. Join the AP and open `http://192.168.4.1/`.
5. If captive auto-open fails, open `http://192.168.4.1/` manually.

## Portal features (v1)

- Wi-Fi scan
- save home SSID/password
- optional hostname update
- network reset
- factory reset

Not included in v1:

- cloud account login
- recipes editor
- OTA portal
- diagnostics console

## Connectivity indicators

- BLE icon/state reflects DE1 BLE path status.
- Wi-Fi state reflects AP-only vs STA-connected state.
- USB indicator appears only when USB is connected.
- Battery indicator appears only when battery telemetry is available, with low/charging state.

## Ring validation behavior (v1)

Physical outer ring rotation does not control brew settings yet.
Each detent triggers a visible UI cue so hardware wiring can be verified quickly.
