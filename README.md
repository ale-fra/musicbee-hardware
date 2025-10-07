# MusicBee Hardware Firmware

MusicBee turns NFC cards into kid-friendly controls for a Google Nest or other networked speaker by bridging an ESP32-based reader with the MusicBee backend service.

## Features
- Tap-to-play card detection with debounce, backend notification, and serial logging.
- Supports MFRC522 (SPI) and PN532 (I²C or SPI) NFC modules selected at compile time.
- Wi-Fi connection management with automatic retry and optional mDNS discovery for `.local` backends.
- Automatic OTA firmware checks on boot and every 24 hours with manifest-driven updates.
- RGB status LED with success, error, and connectivity feedback patterns.
- Optional debug HTTP server for remote visual testing and simulated card scans.
- Configurable NFC command cards for local actions such as resetting the device.

## Hardware Requirements
- ESP32 development board (tested with AZ-Delivery Devkit V4).
- One of the supported NFC readers: MFRC522 or PN532 breakout.
- Common-anode or common-cathode RGB LED wired to GPIO12/13/14 (configurable).
- NFC cards or tags compatible with ISO14443A (MIFARE).

## Wiring
### MFRC522 (SPI) defaults
- SDA/SS → GPIO5 (`NFC_SS_PIN`).
- RST → GPIO27 (`NFC_RST_PIN`).
- Connect the remaining SPI lines (SCK, MOSI, MISO) to the ESP32’s hardware SPI pins and power the module at 3.3 V.

### PN532 (I²C) defaults
- SDA → GPIO21 (`PN532_SDA_PIN`).
- SCL → GPIO22 (`PN532_SCL_PIN`).
- IRQ → GPIO4 (`PN532_IRQ_PIN`).
- RST → GPIO16 (`PN532_RST_PIN`).
- Power the breakout at 3.3 V and share ground with the ESP32.

### PN532 (SPI) defaults
- SS → GPIO5 (`PN532_SS_PIN`).
- SCK → GPIO18 (`PN532_SCK_PIN`).
- MOSI → GPIO23 (`PN532_MOSI_PIN`).
- MISO → GPIO19 (`PN532_MISO_PIN`).
- IRQ → GPIO4 (`PN532_IRQ_PIN`).
- RST → GPIO16 (`PN532_RST_PIN`).
- Power the breakout at 3.3 V and share ground with the ESP32.

### RGB LED
- Red → GPIO12, Green → GPIO13, Blue → GPIO14 (`LED_RED_PIN`, `LED_GREEN_PIN`, `LED_BLUE_PIN`).
- Set `LED_COMMON_ANODE` in `Config.h` if you use a common-anode LED.

## Firmware Configuration
1. Copy `include/secrets.example.h` to `include/secrets.h` and fill in Wi-Fi credentials plus backend host/port constants. Optionally set `SECRET_RESET_CARD_UID` to the uppercase UID of a command card that should reboot the device.
2. Adjust `Config.h` if you change reader type, pin assignments, debounce timing, LED configuration, or enable the debug action server (`ENABLE_DEBUG_ACTIONS` / `DEBUG_SERVER_PORT`). Additional command cards can be added to `ACTION_CARD_MAPPINGS`.

## Build and Flash
1. Install PlatformIO.
2. Connect the ESP32 via USB and choose the environment that matches your NFC reader:
   - `az-delivery-devkit-v4-rc522` for MFRC522 modules.
   - `az-delivery-devkit-v4-pn532` for PN532 modules.
3. Build and upload with the matching environment:
   ```sh
   # MFRC522
   pio run -e az-delivery-devkit-v4-rc522
   pio run -e az-delivery-devkit-v4-rc522 --target upload

   # PN532
   pio run -e az-delivery-devkit-v4-pn532
   pio run -e az-delivery-devkit-v4-pn532 --target upload
```

   These environments set the `USE_RC522` or `USE_PN532` build flags that determine which driver is compiled. Make sure the `NFC_READER_TYPE` option in `include/Config.h` matches the reader and environment you selected. Define `USE_PN532_SPI` (for example by adding `-DUSE_PN532_SPI` to the PN532 environment's `build_flags`) if you wire the PN532 for SPI instead of I²C.

4. Open the serial monitor at 115200 baud to view logs:

   ```sh
   pio device monitor -b 115200
   ```

## Runtime Behavior

* On boot the firmware initializes the RGB LED, connects to Wi-Fi, announces the optional `nfc-jukebox` mDNS name, and runs a brief LED self-test.
* During the main loop it keeps Wi-Fi alive, debounces repeated card reads, checks the OTA manifest every 24 hours, and sends accepted UIDs to the backend API at `/api/v1/cards/{uid}/play`.
* LED feedback indicates state: green blink for success, red for errors, blue for connection attempts.
* Backend responses and errors are printed over serial to help with troubleshooting.

## Debug Action Server

Enable the optional HTTP server by setting `ENABLE_DEBUG_ACTIONS` to `true` in `include/Config.h`. The firmware starts the server on `DEBUG_SERVER_PORT` whenever Wi-Fi is connected, allowing you to trigger effects or simulate NFC scans over the network.

* **List available actions**

  ```http
  GET /debug/actions
  ```

  Response example:

  ```json
  {
    "ok": true,
    "actions": [
      { "name": "set_visual_state", "description": "Set or override the current LED state." },
      { "name": "preview_effect", "description": "Preview an LED effect with custom colours." },
      { "name": "simulate_card", "description": "Simulate an NFC card scan with an arbitrary UID." }
    ]
  }
  ```

* **Set the current LED state**

  ```http
  POST /debug/actions/set_visual_state
  Content-Type: application/json

  {
    "state": "backend_success",
    "apply_to_base": false
  }
  ```

* **Preview a specific effect**

  ```http
  POST /debug/actions/preview_effect
  Content-Type: application/json

  {
    "type": "breathing",
    "r": 255,
    "g": 120,
    "b": 40,
    "period_ms": 900
  }
  ```

* **Simulate a card scan**

  ```http
  POST /debug/actions/simulate_card
  Content-Type: application/json

  {
    "uid": "04A224D9123480",
    "bypass_debounce": true,
    "send_to_backend": false
  }
  ```

The action responses share a common envelope (`{"ok":true/false,"message":"..."}`) and return 4xx status codes for invalid JSON or unknown actions.

## Troubleshooting

* If mDNS lookup fails, verify the backend advertises a `.local` hostname and that both devices share the same network.
* Connection failures typically mean the backend is offline, the address/port are wrong, or network/firewall rules block the request.
* Persistent Wi-Fi drops are retried automatically; check your credentials and signal strength if reconnecting takes too long.

