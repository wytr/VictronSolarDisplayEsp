# VictronSolarDisplayEsp

An ESP32-S3-based solar panel monitor that displays real-time data from a Victron BLE panel using LVGL, and includes a built-in Wi‑Fi AP configuration server to set the AES encryption key via a user-friendly web interface or directly on the device.

**Works with Victron SmartSolar chargers and Victron Battery Monitor devices that broadcast Bluetooth telemetry (SmartShunt, BMV series, etc.).**

---
## Screenshots

Below are screenshots of the device UI, showing both the Live tab and various Settings tab configuration screens:

<p align="center">
  <img src="docs/images/victrondata.png" alt="Live Tab: Victron Data Overview" width="260" style="margin: 0 12px; display: inline-block;"/>
  <br/>
  <b>Live Tab:</b> Shows real-time Victron data including battery voltage, current, load, solar yield, and system state.
</p>

<p align="center">
  <img src="docs/images/ap-config.png" alt="Settings Tab: AP Config" width="260" style="margin: 0 12px; display: inline-block;"/>
  <br/>
  <b>Settings Tab – AP Config:</b> Configure the Wi-Fi AP SSID, password, and enable/disable the access point.
</p>

<p align="center">
  <img src="docs/images/screensaver.png" alt="Settings Tab: Screensaver Settings" width="260" style="margin: 0 12px; display: inline-block;"/>
  <br/>
  <b>Settings Tab – Screensaver:</b> Adjust screensaver enable, brightness, and timeout settings.
</p>

<p align="center">
  <img src="docs/images/mac-and-aes.png" alt="Settings Tab: MAC and AES Key" width="260" style="margin: 0 12px; display: inline-block;"/>
  <br/>
  <b>Settings Tab – MAC & AES Key:</b> View and edit the AES key and see the current BLE MAC address. Save or reboot from here.
</p>

-------

<p align="center">
  <img src="docs/images/battery-monitor-1-1-0.png" alt="V1.1.0 Battery Monitor Ui" width="260" style="margin: 0 12px; display: inline-block;"/>
  <br/>
  <b>NEW Battery Monitor Ui</b>
</p>

---
## Features

- **BLE Decryption & Display**
  - Scans Victron BLE advertisements, decrypts them with a user-configurable 128-bit AES key, and parses Solar Charger (record type 0x01) and Battery Monitor (record type 0x02) payloads including voltage, current, PV yield, state-of-charge, time-to-go, and auxiliary metrics.
  - Displays live data on a 320x480 (rotated) LCD using LVGL (Default Dark-Theme).
  - Shows device state and error codes with icons and text.
  - Displays the MAC address of the currently connected Victron BLE device.
  - Adapts the UI copy and layout based on the detected device class so charger-only fields stay hidden when a battery monitor is active.

- **On‑Device Configuration**
  - **Web Interface:** Creates a Wi‑Fi SoftAP (`VictronConfig`) on boot. Hosts a web page (SPIFFS) for entering a new AES key.
  - **On-Device UI:** The Settings tab allows direct entry and saving of the AES key and displays the current key and BLE MAC address.
  - All configuration is stored persistently in NVS.

- **Persistent Storage**
  - AES key, Wi-Fi settings, and display brightness are stored in NVS.
  - Default AES key is used if none is set by the user.

## Latest Release

- **1.1.0** adds full Victron Battery Monitor (record type 0x02) support alongside Solar Charger (record type 0x01) telemetry with automatic UI updates.
- Download prebuilt artifacts (`bootloader.bin`, `partition-table.bin`, `VictronSolarDisplayEsp.bin`, `spiffs.bin`, `flasher_args.json`, `README.md`) from the GitHub Releases page and flash them with the offsets shown below.
- Use the demo page at [https://demo.nihilanth.de/](https://demo.nihilanth.de/) to flash the firmware directly from Chrome or Microsoft Edge without installing local tooling (except usb driver, youll need that one).
- Verify Battery Monitor metrics (SOC, TTG, currents) after flashing to confirm your AES key and device pairing.

---

## Hardware Requirements

<p align="center">
  <img src="docs/images/guition.webp" alt="AES Key Configuration Captive Portal" width="600" style="margin: 0 12px; display: inline-block;"/>
  <br/>
  <b>Guition JC3248W535 3.5"</b> ESP32-S3 capacitive touch display module (320×480)
</p>
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
   ├─ ui.c                  # Core LVGL wiring + device view orchestration
   └─ ui/                   # Modular UI components (views, helpers)
      ├─ view_solar.c       # Solar charger layout & update logic
      ├─ view_battery.c     # Battery monitor layout & update logic
      ├─ view_registry.c    # Factory registry for victron_record_type_t
      ├─ settings_panel.c   # Settings-tab widgets and event handlers
      ├─ ui_state.h         # Shared UI state struct
      └─ ui_format.c        # Common label formatting helpers
   ├─ display.h/.c          # LCD BSP interaction
   ├─ config_storage.c      # NVS read/write for AES key, Wi-Fi, brightness
   └─ ...                   # Other headers & components
```

---

## Adding Support for a New Victron Device Type

The UI is now modular: each device view lives in `main/ui/` and is registered by type. To plug in a new Victron payload, follow these steps:

1. **Update BLE Parsing**
   - Extend `victron_ble.c` to decode the new record type into `victron_data_t`. Add any required structs to `victron_ble.h` and assign a unique `victron_record_type_t` enum value.

2. **Implement the View Module**
   - Create `main/ui/view_<device>.c` with a `ui_device_view_t` implementation (see `view_solar.c` or `view_battery.c` for patterns).
     - Build LVGL widgets under the provided parent, using the shared styles in `ui_state_t`.
     - Implement `update()` to format incoming data, reusing helpers from `ui_format.c` where possible.
     - Provide `show()`/`hide()` for visibility toggling; `destroy()` only if you allocate additional resources.

3. **Register the View**
   - Add an entry to `main/ui/view_registry.c` mapping your new `victron_record_type_t` to the module’s factory function and display name.
   - If the device should appear with a specific label in the Settings tab, also update `ui_settings_panel_init()` as needed.

4. **Expose Shared State**
   - If the view requires additional shared UI state, extend `ui_state_t` in `main/ui/ui_state.h` and initialise it in `ui_init()`.

5. **Test End-to-End**
   - Build (`idf.py build`) and flash the firmware.
   - Feed BLE payloads from the new device type (or a mock) and confirm the layout updates correctly without regressions for existing devices.

Following this flow keeps `ui.c` untouched and ensures each device type remains self-contained and swappable via the registry.

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
   - **Captive Portal:** When you connect with an Android or iPhone, a popup will automatically appear, directing you to the configuration page (index.html). This makes setup fast and easy—no need to manually enter the IP address!
   - You can also browse to [http://192.168.4.1/](http://192.168.4.1/) to configure the AES key via web UI.
   - Alternatively, use the Settings tab on the device to enter and save the AES key.
   - After saving, the device reboots and begins displaying live BLE data.

---

## Framebuffer Screenshot Conversion

You can capture a raw framebuffer screenshot from the device and convert it to a PNG image using the provided Python script.

### 1. Download the raw framebuffer

Use `curl` to fetch the framebuffer from the device's HTTP endpoint (replace the IP if needed):

```bash
curl http://192.168.4.1/screenshot -o framebuffer.raw
```

### 2. Convert to PNG

Run the Python script to convert the raw framebuffer to a PNG image:

```bash
python convert-framebuffer.py framebuffer.raw output.png
```

---

## Configuration

- **Default AES key (if none set):**  
  `4B7178E64C828A262CDD5161E3404B7A`
- **To change the AES key:**
  - Connect to the AP, enter a new 32-character hex string in the web UI, and click **Save**.
  - Or, use the Settings tab on the device, enter the new key, and press **Save**.
- **Other settings:**
  - Wi-Fi SSID, password, and AP enable/disable can be configured from the Settings tab.
  - Display brightness is adjustable and persists across reboots.

---

## Dependencies

- [ESP-IDF v5.4.1](https://docs.espressif.com/projects/esp-idf)
- [NimBLE](https://github.com/apache/mynewt-nimble)
- [LVGL](https://lvgl.io/) and Espressif LVGL port
- AES CTR mode from `esp_aes`

---

## Device UI (Settings Tab)

- **AP SSID / Password:** Configure Wi-Fi AP settings.
- **Enable AP:** Checkbox to enable/disable the SoftAP.
- **Error:** Shows the latest error code from Victron BLE data.
- **MAC Address:** Shows the MAC address of the currently connected Victron BLE device.
- **AES Key:** Shows the current AES key. You can enter a new key and press **Save** to update.
- **Save / Reboot:** Save the AES key or reboot the device.
- **Brightness:** Adjust the display brightness (persists in NVS).

---

## AES Key Configuration via Captive Portal

On first boot (or after a reset), the device creates a Wi-Fi access point (`VictronConfig`) and launches a captive portal. When you connect with your phone or computer, a popup or redirect will automatically open the configuration page. Here you can securely enter or update the 16-byte AES key (32 hex characters) required for decrypting Victron BLE data.

<p align="center">
  <img src="docs/images/keyconfig_mobile.png" alt="AES Key Configuration Captive Portal" width="260" style="margin: 0 12px; display: inline-block;"/>
  <br/>
  <b>Captive Portal – AES Key Configuration:</b> Enter your 32-character AES key directly from your mobile device or computer. The interface is mobile-friendly and guides you through the process.
</p>

After saving the key, the device will reboot and begin displaying live data.

---
## Hardware Build Guide

Assembly notes and 3D printed enclosure contributed by [NomadicNico](https://github.com/NomadicNico) – thank you!

### Bill of Materials

- 3D printed enclosure: [Guition JC3248W535 3.5" Surface Mount Case](https://www.printables.com/model/1442589-guition-jc3248w535-35-surface-mount-case)
- 4‑pole 1.25 mm connector with pre-crimped leads: [Elechawk PicoBlade-compatible cable set](https://www.amazon.de/dp/B07S18D3RN)
- DC buck converter:
  - Adjustable: [TECNOIOT Mini-360 DC-DC Step Down](https://www.amazon.de/-/en/TECNOIOT-Mini360-Mini-360-Adjustable-Converter/dp/B07G4FM6ZW)
  - Fixed 5 V: [LAOMAO Mini 560 Step Down](https://www.amazon.de/-/en/Converter-Voltage-LAOMAO-Output-Modules/dp/B0B92ZDK6T)
- Hook-up wire, heat-shrink tubing, and double-sided mounting tape (e.g. 3M VHB)

### Wiring & Assembly

1. **Prepare the connector harness.** Insert red and black pre-crimped leads into the 4-pole plug and trim to ~50 mm.  
   ![Pre-crimped harness](docs/images/precrimped.png)

2. **Solder the 5 V output.** Slide heat-shrink onto each lead, solder the red wire to Vout+ and black wire to Vout− on the buck converter, then shrink for strain relief.  
   ![Buck output soldering](docs/images/soldered-buck.png)

3. **Set the input wiring.** Add leads to Vin+ and Vin−, insulate with heat-shrink, and (for adjustable modules) dial the output to exactly 5 V before connecting the display.

4. **Enclose the converter.** Wrap the buck converter with heat-shrink so no metal is exposed and ready it for mounting.

5. **Mount the display.** Install the display into the printed case.  
   ![Display assembled in printed case](docs/images/display-3d-print.png)

6. **Route power and secure the buck.** Plug in the 4-pole connector, position the buck converter centrally over the wall bracket cut-out, and secure it with mounting tape.  
   ![Buck positioned and connected](docs/images/display-connect-buck.png)

7. **Install the wall bracket.** Fix the bracket in the desired location (example: Hymer instrument panel using the supplied adapter plate).  
   ![Mounted display in Hymer instrument panel](docs/images/display-mount-hymer.png)

8. **Provide 12 V supply.** Run the Vin leads through the mounting hole and tie into a fused 12 V source.

9. **Attach the display.** Press the display onto the bracket until it clicks. To remove it later, pull firmly or lever the side tabs via the case slots.  
   ![Finished installation showcase](docs/images/display-mount-hymer-showcase.png)

---
## Victron BLE Manufacturer Data Support (Planned Improvements)

This project currently supports parsing and decrypting Victron BLE advertisements for record types 0x01 (Solar Charger) and 0x02 (Battery Monitor). The Victron BLE protocol, as described in the official documentation (see [`docs/extra-manufacturer-data-2022-12-14.pdf`](docs/extra-manufacturer-data-2022-12-14.pdf)), defines several record types with different encrypted payloads:

| Value | Record Type |
|-------|--------------|
| 0x00 | Test record |
| 0x01 | Solar Charger |
| 0x02 | Battery Monitor |
| 0x03 | Inverter |
| 0x04 | DC/DC converter |
| 0x05 | SmartLithium |
| 0x06 | Inverter RS |
| 0x07 | GX-Device (Record layout TBD) |
| 0x08 | AC Charger (Record layout TBD) |
| 0x09 | Smart Battery Protect |
| 0x0A | (Lynx Smart) BMS |
| 0x0B | Multi RS |
| 0x0C | VE.Bus |
| 0x0D | DC Energy Meter |

**Planned future improvements:**
- Support for parsing and decrypting all Victron BLE record types (BMS, Inverter, Charger, VE.Direct LoRaWAN, etc.)
- Dynamic handling of encrypted data length and payload struct based on record type
- Proper struct definitions and parsing for each decrypted payload type
- Extensible callback or dispatch mechanism for different record types

If you are interested in contributing or need support for a specific record type, please open an issue or pull request!

---

## License

This project is released under the GNU General Public License v3.0 (GPLv3). See `LICENSE` for details.
