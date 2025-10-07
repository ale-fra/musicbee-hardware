/*
 * Example secrets header for the NFC jukebox firmware.
 *
 * Copy this file to `secrets.h` and fill in your sensitive values.
 * The real `secrets.h` should NOT be tracked in version control (see
 * `.gitignore`). Keeping secrets separate helps prevent accidentally
 * committing credentials to a public repository.
 */

#pragma once

// Replace the following placeholders with your actual Wi‑Fi network
// credentials. These constants are used by Config.h. Do not wrap
#define SECRET_WIFI_SSID "XXXXX"
#define SECRET_WIFI_PASSWORD "XXXX"

// Backend server configuration. Provide the hostname (or IP address)
// and port of your jukebox backend service. For example, if your
// backend is running on a Raspberry Pi at 192.168.1.100 port 3000,
// define SECRET_BACKEND_HOST as "192.168.1.100" and
// SECRET_BACKEND_PORT as 3000.
#define SECRET_BACKEND_HOST   "musicbee.local"
#define SECRET_BACKEND_PORT   8080

// Optional: UID for the reset command card. Provide the uppercase hexadecimal
// UID exactly as read by the firmware. Leave the string empty to disable the
// reset card.
#define SECRET_RESET_CARD_UID ""

