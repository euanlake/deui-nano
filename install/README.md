# DEUI Nano browser install page

Static GitHub Pages site for one-time USB flashing via [ESP Web Tools](https://esphome.github.io/esp-web-tools/).

## Local preview

Serve the `install/` directory over HTTP (WebSerial requires a secure context or localhost):

```bash
cd install
python3 -m http.server 8080
```

Open `http://localhost:8080/` in Chrome or Edge.

## CI

On each push to `main` that changes firmware or install assets, `.github/workflows/firmware-ci.yml`:

1. Builds `deui_nano.bin` and a merged full-flash image
2. Copies the merged binary to `install/firmware/`
3. Updates `install/manifest.json` and deploys to GitHub Pages

Tagged releases (`v*.*.*`) still publish OTA binaries and update `firmware/esp32/ota/manifest.json` via `.github/workflows/firmware-release.yml`. The manifest commit to `main` then triggers the Pages deploy above automatically.

Published URL: `https://euanlake.github.io/deui-nano/`

## Development notes

- `manifest.json` follows the ESP Web Tools schema (`chipFamily`, `parts[]`, `offset`)
- Styling follows the Deui marketing site (`styles.css` ported from `deui website/src/index.css`)
- Theme-aware assets: logos, controller display mockup, favicons, and full Lab Grotesque font set under `assets/`, `favicon/`, and `fonts/LabGrotesque/`
- `install/firmware/*.bin` is gitignored; CI publishes merged full-flash images to GitHub Pages on release tags
