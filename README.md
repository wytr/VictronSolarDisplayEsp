# VictronSolarDisplayEsp

An ESP32-S3-based solar panel monitor that displays real-time data from a Victron BLE panel using LVGL, and includes a built-in Wi‑Fi AP configuration server to set the AES encryption key via a user-friendly web interface or directly on the device.

---

## Features

- **BLE Decryption & Display**
  - Scans Victron BLE advertisements, decrypts them with a user-configurable 128‑bit AES key, and parses output voltage, current, solar input, and yield.
  - Displays live data on a 320x480 (rotated) LCD using LVGL and custom UI themes.
  - Shows device state and error codes with icons and text.
  - Displays the MAC address of the currently connected Victron BLE device.

- **On‑Device Configuration**
  - **Web Interface:** Creates a Wi‑Fi SoftAP (`VictronConfig`) on boot. Hosts a web page (SPIFFS) for entering a new AES key.
  - **On-Device UI:** The Info tab allows direct entry and saving of the AES key and displays the current key and BLE MAC address.
  - All configuration is stored persistently in NVS.

- **Persistent Storage**
  - AES key, Wi-Fi settings, and display brightness are stored in NVS.
  - Default AES key is used if none is set by the user.

---

## Hardware Requirements

- **ESP32-S3** development board with PSRAM (≥8 MB recommended)
- **JC3248W535** capacitive touch display module (320×480) from GuitIon
- Power supply: 5 V USB or battery with stabilizer

---

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
   ├─ config_storage.c      # NVS read/write for AES key, Wi-Fi, brightness
   └─ ...                   # Other headers & components
```

---

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

   - On first boot, the device sets up a SoftAP `VictronConfig` (no password). Connect to it.
   - Browse to [http://192.168.4.1/](http://192.168.4.1/) to configure the AES key via web UI.
   - Alternatively, use the Info tab on the device to enter and save the AES key.
   - After saving, the device reboots and begins displaying live BLE data.

---

## Configuration

- **Default AES key (if none set):**  
  `4B7178E64C828A262CDD5161E3404B7A`
- **To change the AES key:**
  - Connect to the AP, enter a new 32-character hex string in the web UI, and click **Save**.
  - Or, use the Info tab on the device, enter the new key, and press **Save**.
- **Other settings:**
  - Wi-Fi SSID, password, and AP enable/disable can be configured from the Info tab.
  - Display brightness is adjustable and persists across reboots.

---

## Dependencies

- [ESP-IDF v5.4.1](https://docs.espressif.com/projects/esp-idf)
- [NimBLE](https://github.com/apache/mynewt-nimble)
- [LVGL](https://lvgl.io/) and Espressif LVGL port
- AES CTR mode from `esp_aes`

---

## Device UI (Info Tab)

- **AP SSID / Password:** Configure Wi-Fi AP settings.
- **Enable AP:** Checkbox to enable/disable the SoftAP.
- **Error:** Shows the latest error code from Victron BLE data.
- **MAC Address:** Shows the MAC address of the currently connected Victron BLE device.
- **AES Key:** Shows the current AES key. You can enter a new key and press **Save** to update.
- **Save / Reboot:** Save the AES key or reboot the device.
- **Brightness:** Adjust the display brightness (persists in NVS).

---

## License

This project is released under the MIT License. See `LICENSE` for details.
