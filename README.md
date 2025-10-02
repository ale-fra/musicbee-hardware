# MusicBee Hardware Firmware

MusicBee turns NFC cards into kid-friendly controls for a Google Nest or other networked speaker by bridging an ESP32-based reader with the MusicBee backend service.

## Features
- Tap-to-play card detection with debounce, backend notification, and serial logging.
- Supports MFRC522 (SPI) and PN532 (I²C) NFC modules selected at compile time.
- Wi-Fi connection management with automatic retry and optional mDNS discovery for `.local` backends.
- RGB status LED with success, error, and connectivity feedback patterns.

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

### RGB LED
- Red → GPIO12, Green → GPIO13, Blue → GPIO14 (`LED_RED_PIN`, `LED_GREEN_PIN`, `LED_BLUE_PIN`).
- Set `LED_COMMON_ANODE` in `Config.h` if you use a common-anode LED.

## Firmware Configuration
1. Copy `include/secrets.example.h` to `include/secrets.h` and fill in Wi-Fi credentials plus backend host/port constants.
2. Adjust `Config.h` if you change reader type, pin assignments, debounce timing, or LED configuration.

## Build and Flash
1. Install PlatformIO.
2. Connect the ESP32 via USB and select the `az-delivery-devkit-v4` environment defined in `platformio.ini`.
3. Build and upload:
   ```sh
   pio run
   pio run --target upload
```

4. Open the serial monitor at 115200 baud to view logs:

   ```sh
   pio device monitor -b 115200
   ```

## Runtime Behavior

* On boot the firmware initializes the RGB LED, connects to Wi-Fi, announces the optional `nfc-jukebox` mDNS name, and runs a brief LED self-test.
* During the main loop it keeps Wi-Fi alive, debounces repeated card reads, and sends accepted UIDs to the backend API at `/api/v1/cards/{uid}/play`.
* LED feedback indicates state: green blink for success, red for errors, blue for connection attempts.
* Backend responses and errors are printed over serial to help with troubleshooting.

## Troubleshooting

* If mDNS lookup fails, verify the backend advertises a `.local` hostname and that both devices share the same network.
* Connection failures typically mean the backend is offline, the address/port are wrong, or network/firewall rules block the request.
* Persistent Wi-Fi drops are retried automatically; check your credentials and signal strength if reconnecting takes too long.

