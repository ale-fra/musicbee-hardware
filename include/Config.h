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
// types are supported in the future. The PlatformIO environment
// declares USE_RC522 (default) or USE_PN532 to select the appropriate
// driver, and this block mirrors that choice.
enum class RfidHardwareType { RC522, PN532 };

#if defined(USE_PN532_SPI) && !defined(USE_PN532)
#  error "USE_PN532_SPI requires USE_PN532 to also be defined."
#endif

#if defined(USE_PN532) && defined(USE_RC522)
#  error "Only one of USE_PN532 or USE_RC522 may be defined."
#elif defined(USE_PN532)
static constexpr RfidHardwareType NFC_READER_TYPE = RfidHardwareType::PN532;
#elif defined(USE_RC522)
static constexpr RfidHardwareType NFC_READER_TYPE = RfidHardwareType::RC522;
#else
#  error "Define USE_RC522 (default) or USE_PN532 to select an NFC reader."
#endif

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

// PN532 (SPI) wiring defaults. Define USE_PN532_SPI to activate these pins
// and the SPI transport in the firmware.

static constexpr uint8_t PN532_SS_PIN   = 10;  // SPI slave select (CS)
static constexpr uint8_t PN532_SCK_PIN  = 12;  // SPI clock
static constexpr uint8_t PN532_MOSI_PIN = 11;  // SPI MOSI
static constexpr uint8_t PN532_MISO_PIN = 13;  // SPI MISO

// Addressable LED strip configuration (WS2812/NeoPixel)
static constexpr uint8_t  LED_DATA_PIN           = 48;  // Data in to the strip
static constexpr uint16_t LED_COUNT_DEFAULT      = 11;  // Number of pixels in the strip
static constexpr uint8_t  LED_BRIGHTNESS_DEFAULT = 200; // 0-255 brightness scaling

// Debounce interval (ms) to ignore repeated reads of the same card.
static constexpr unsigned long CARD_DEBOUNCE_MS = 800;

// Wi‑Fi reconnection settings. Adjust if you need to tune how
// aggressively the device should retry connections.
static constexpr unsigned long WIFI_RETRY_DELAY_MS = 2000;
static constexpr uint8_t       MAX_WIFI_RETRIES     = 20;


// Legacy discrete RGB LED configuration removed; use the NeoPixel strip instead.
