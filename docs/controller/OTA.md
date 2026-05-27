# OTA updates (DEUI Nano)

HTTPS over-the-air updates for the ESP32-S3 controller using dual OTA partitions, GitHub Releases, and a repo-hosted manifest.

## How updates work

1. Each boot/wake cycle runs **one** automatic manifest check immediately before deep sleep (after the 5-minute idle timeout).
2. If a newer semver is published and the device has home Wi-Fi (STA + IP), firmware downloads via `esp_https_ota`, writes the inactive OTA slot, and reboots.
3. After reboot, the new image runs in **pending verify** state until 30 seconds of successful uptime, then calls `esp_ota_mark_app_valid_cancel_rollback()`. If the device crashes before that, the bootloader rolls back to the previous slot.
4. Users can also trigger a check from the Wi-Fi portal: **Setup → Software Update → Check for updates**.

## Manifest URL

Devices poll:

`https://raw.githubusercontent.com/euanlake/deui-nano/main/firmware/esp32/ota/manifest.json`

CI updates this file on each tagged release (`vX.Y.Z`).

Schema:

```json
{
  "product": "deui_nano",
  "target": "esp32s3",
  "version": "1.0.0",
  "url": "https://github.com/euanlake/deui-nano/releases/download/v1.0.0/deui_nano-esp32s3-v1.0.0.bin",
  "sha256": "<hex>",
  "notes": "Release notes"
}
```

## Portal Software Update screen

| Route | Purpose |
|-------|---------|
| `/updates` | Current version + **Check for updates** |
| `POST /api/ota-check` | Start portal-triggered check (background task) |
| `GET /api/ota-status` | JSON status for UI polling |

Portal manual checks **skip** setup-AP deferral (user is already on the portal) but still require STA internet. Auto checks defer while a phone is joining the setup AP, DHCP is pending, or DE1 is extracting.

## Partition layout

Dual OTA (`ota_0` / `ota_1`, 3 MB each) replaces the old single `factory` slot. **Migration requires a full USB flash** — OTA cannot change the partition table.

```bash
cd firmware/esp32
./dev.sh full
```

## Kconfig

In `idf.py menuconfig` → **DEUI OTA**:

- `CONFIG_DEUI_OTA_ENABLE` — master switch
- `CONFIG_DEUI_OTA_MANIFEST_URL` — manifest URL override

## First install (DIY)

Browser install page (ESP Web Tools):

`https://euanlake.github.io/deui-nano/`

See [FLASHING_CONTROLLER.md](FLASHING_CONTROLLER.md) and [../install/README.md](../../install/README.md).

## Releasing firmware

1. Bump `project(deui_nano VERSION x.y.z)` in `firmware/esp32/CMakeLists.txt`
2. Tag and push: `git tag vX.Y.Z && git push origin vX.Y.Z`
3. GitHub Actions builds, publishes the Release asset, updates manifests, and deploys the install page

## Troubleshooting

| Symptom | Likely cause |
|---------|----------------|
| Auto check never downloads | No home Wi-Fi yet, setup AP session active, or DE1 extracting — wait for next wake cycle |
| Portal says "Connect to home Wi-Fi first" | STA has no IP (still on setup AP only) |
| Device reverts after OTA | New firmware crashed before 30 s mark_valid window |
| OTA fails immediately | Manifest URL unreachable, bad SHA256, or binary too large for 3 MB slot |
| Old units won't OTA | Still on `factory` partition — run `./dev.sh full` once |

## Security notes

- HTTPS with ESP-IDF default CA bundle (`CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL`)
- Manifest includes SHA256 (computed by CI; on-device verify is optional in v1)
- Secure boot and certificate pinning are **not** enabled in v1
