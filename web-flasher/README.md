# CrossPoint Web Flasher & GitHub Pages

Web-based firmware flasher and live serial monitor for CrossPoint Reader e-ink devices.

Powered by [ESP Web Tools](https://esphome.github.io/esp-web-tools/) and [esptool-js](https://github.com/espressif/esptool-js) using the Web Serial API.

---

## Features

- **⚡ One-Click Web Installer**: Select your device (X4Pro, X3/X4, Sticky, X4C, M5PaperMono) and flash with a single click in Chromium browsers (Chrome, Edge, Brave, Opera).
- **💾 Custom Binary Flasher**: Drag and drop any `.bin` file, set custom flash offset (default `0x10000`), choose baud rate, and flash with real-time progress.
- **📟 Live Serial Monitor**: Stream debug logs, FreeInkUI logs, and Lua serial output in real time with hardware reset capabilities.
- **📥 Direct Downloads**: Download prebuilt `.bin` firmware files directly for command-line `esptool.py` or SD-card OTA recovery.

---

## Local Development & Testing

Web Serial API requires HTTPS or `localhost`. To test the web flasher locally:

```bash
cd web-flasher
python3 -m http.server 8000
```

Open `http://localhost:8000` in Google Chrome or Microsoft Edge.

---

## Automated Deployment (GitHub Actions)

The GitHub Actions workflow (`.github/workflows/build-and-pages.yml`) handles:
1. Compiling firmware for all target devices (`x4pro`, `default`, `sticky`, `x4c`, `papermono`) via PlatformIO.
2. Collecting compiled `.bin` artifacts.
3. Updating manifests and `version.json` with commit SHA, branch, and build timestamp.
4. Packaging and deploying the site to **GitHub Pages** (`https://<username>.github.io/crosspoint-reader/`).

---

## Flash Offsets Reference

- **`0x10000` (65536)**: CrossPoint application partition (`app0`). Used for firmware updates.
- **`0x8000` (32768)**: Partition table (`partitions.bin`).
- **`0x0` (0)**: ESP32-S3 / ESP32-C3 bootloader (`bootloader.bin`).
