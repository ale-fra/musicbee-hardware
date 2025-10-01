/*
 * WifiManager.h
 *
 * Provides a simple wrapper around the Arduino WiFi library to
 * establish and maintain a Wi‑Fi connection. It attempts to join
 * the configured network and will periodically check the connection
 * status, reconnecting automatically if dropped. It also exposes a
 * method to test the connection before performing network requests.
 */

#pragma once

#include <WiFi.h>
#include "Config.h"

class WifiManager {
public:
  /**
   * Initialise the Wi‑Fi interface and attempt to connect using the
   * credentials defined in Config.h.
   *
   * Returns true if the connection succeeds within the maximum
   * allowed retries, false otherwise.
   */
  bool begin();

  /**
   * Ensure the ESP remains connected. If the connection has dropped
   * this method will try to reconnect. Call this periodically in
   * loop() to keep the device online.
   */
  void loop();

  /**
   * Returns true if the ESP is currently connected to Wi‑Fi.
   */
  bool isConnected() const;

private:
  unsigned long _lastCheckMillis = 0;
};