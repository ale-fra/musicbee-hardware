/*
 * Global compile‑time configuration values for the jukebox firmware.
 *
 * This header is intentionally minimal and contains only constants that
 * should rarely change between deployments. Secrets such as Wi‑Fi
 * credentials and backend host details are defined in `secrets.h`,
 * which is excluded from version control. See `secrets.example.h` for
 * an example of the required definitions.
 */

#pragma once

#include <Arduino.h>
#include "secrets.h"

// Wi‑Fi credentials are provided by secrets.h via SECRET_WIFI_SSID and
// SECRET_WIFI_PASSWORD. If you wish to configure a fallback SSID or
// password at compile time, you can define them here instead of in
// secrets.h (not recommended).

static constexpr const char *const WIFI_SSID     = SECRET_WIFI_SSID;
static constexpr const char *const WIFI_PASSWORD = SECRET_WIFI_PASSWORD;

// Backend host configuration. Provide the server name or IP and port
// where your Node.js/Express jukebox backend is running. These values
// come from secrets.h as well. Avoid hard‑coding them here; instead
// update secrets.h so they remain outside of source control.
static constexpr const char *const BACKEND_HOST  = SECRET_BACKEND_HOST;
static constexpr uint16_t         BACKEND_PORT  = SECRET_BACKEND_PORT;

// RFID/NFC reader selection. Choose which hardware backend should be
// compiled into the firmware. Add new enum values if additional reader
// types are supported in the future.
enum class RfidHardwareType { RC522, PN532 };

// Select the active reader for this build. The default remains the
// MFRC522 as it was the original hardware target. Change this constant
// to `RfidHardwareType::PN532` when wiring a PN532 breakout instead.
static constexpr RfidHardwareType NFC_READER_TYPE = RfidHardwareType::RC522;

// Hardware pin definitions. These defaults correspond to common ESP32
// development board layouts. Update them to match your wiring. The
// RC522 uses SPI while the PN532 is configured for I²C by default.
static constexpr uint8_t NFC_SS_PIN  = 5;   // SPI SS pin (SDA/SS) for RC522
static constexpr uint8_t NFC_RST_PIN = 27;  // Reset pin for RC522

// PN532 (I²C) wiring defaults. IRQ and reset are optional on some
// boards, but defining them here keeps the setup consistent. SDA/SCL
// default to the standard ESP32 I²C pins.
static constexpr uint8_t PN532_IRQ_PIN  = 4;   // IRQ pin from PN532 to ESP32
static constexpr uint8_t PN532_RST_PIN  = 16;  // Optional PN532 reset pin
static constexpr uint8_t PN532_SDA_PIN  = 21;  // I²C SDA
static constexpr uint8_t PN532_SCL_PIN  = 22;  // I²C SCL

static constexpr uint8_t LED_PIN     = 2;   // On‑board LED (GPIO2 on most ESP32)

// Debounce interval (ms) to ignore repeated reads of the same card.
static constexpr unsigned long CARD_DEBOUNCE_MS = 800;

// Wi‑Fi reconnection settings. Adjust if you need to tune how
// aggressively the device should retry connections.
static constexpr unsigned long WIFI_RETRY_DELAY_MS = 2000;
static constexpr uint8_t       MAX_WIFI_RETRIES     = 20;


// RGB LED pins - adjust these to match your wiring
// Common pin should be connected to GND (Common Cathode) or VCC (Common Anode)
static constexpr uint8_t LED_RED_PIN   = 12;   // Red channel
static constexpr uint8_t LED_GREEN_PIN = 13;   // Green channel  
static constexpr uint8_t LED_BLUE_PIN  = 14;  // Blue channel
static constexpr bool    LED_COMMON_ANODE = false; // Set to true if Common Anode
