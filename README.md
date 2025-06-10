# VictronSolarDisplayEsp

An ESP32-S3-based solar panel monitor that displays real-time data from a Victron BLE panel using LVGL, and includes a built-in Wi‑Fi AP configuration server to set the AES encryption key via a user-friendly web interface.

## Features

* **BLE Decryption & Display**

  * Scans Victron BLE advertisements, decrypts them with a user-configurable 128‑bit AES key, and parses output voltage, current, solar input, and yield.
  * Displays live data on an 320x480 (rotated) LCD using LVGL and custom UI themes.
  * Shows device state and error codes with icons and text.

* **On‑Device Configuration Server**

  * Creates a Wi‑Fi SoftAP (`VictronConfig`) on boot.
  * Hosts a web page (SPIFFS) serving HTML/CSS/JS assets under `/spiffs`.
  * Form at `/` to enter a 32‑hex‑character AES key; saves to NVS and reboots.
  * Static file handler serves `/style.css`, `/js/jquery…`, `favicon.ico`, etc.

* **Persistent Storage**

  * AES key stored in NVS under the `aes_key` namespace.
  * Default key used if none set by the user.

## Hardware Requirements

* **ESP32-S3** development board with PSRAM (≥8 MB recommended)
* **JC3248W535** capacitive touch display module (320×480) from GuitIon
* Power supply: 5 V USB or battery with stabilizer

## Project Structure

```
VictronSolarDisplayEsp/
├─ CMakeLists.txt           # Top-level, includes spiffs partition
├─ sdkconfig                # IDF configuration
├─ files/                   # Static web assets (SPIFFS)
│   ├─ index.html
│   ├─ style.css
│   ├─ favicon.ico
│   ├─ git-icon.svg
│   └─ js/
│      └─ jquery-3.7.1.js
└─ main/                    # ESP-IDF application
   ├─ CMakeLists.txt        # Component registration
   ├─ main.c                # app_main, LVGL init, start services
   ├─ config_server.c       # Wi-Fi AP + HTTP server for config
   ├─ victron_ble.c         # BLE scanning & AES decryption
   ├─ ui.c                  # LVGL UI definition and callbacks
   ├─ display.h/.c          # LCD BSP interaction
   ├─ config_storage.c      # NVS read/write for AES key
   └─ ...                   # Other headers & components
```

## Build & Flash

1. **Install ESP-IDF v5.4.1** and set up environment:

   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```

2. **Prepare static assets**: edit `files/index.html`, `style.css`, add any assets under `files/`.

3. **Full clean & build**:

   ```bash
   idf.py fullclean build
   ```

4. **Flash firmware + partitions** (includes SPIFFS image):

   ```bash
   idf.py flash monitor
   ```

5. **Interact**:

   * On first boot, the device sets up a SoftAP `VictronConfig` (no password). Connect to it.
   * Browse to `http://192.168.4.1/` to configure the AES key.
   * After saving, the device reboots and begins displaying live BLE data.

## Configuration Key

* **Default AES key (if none set)**: `4B7178E64C828A262CDD5161E3404B7A`
* To change it, connect to the AP, enter a new 32-character hex string, and click **Save**.

## Dependencies

* [ESP-IDF v5.4.1](https://docs.espressif.com/projects/esp-idf)
* [NimBLE](https://github.com/apache/mynewt-nimble)
* [LVGL](https://lvgl.io/) and Espressif LVGL port
* AES CTR mode from `esp_aes`

## License

This project is released under the MIT License. See `LICENSE` for details.
