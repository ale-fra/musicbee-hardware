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

// Hardware pin definitions. These default values correspond to a
// typical ESP32 dev kit wired to an MFRC522 module via SPI and using
// the built‑in LED for feedback. Update these constants to match
// your wiring. If your previous project used different GPIOs, set
// them here accordingly to preserve compatibility.
static constexpr uint8_t NFC_SS_PIN  = 5;   // SPI SS pin (SDA/SS) for RC522
static constexpr uint8_t NFC_RST_PIN = 27;  // Reset pin for RC522
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
