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
   * Initialise the Wi‑Fi interface and kick off a connection attempt
   * using the credentials defined in Config.h. Returns true only if the
   * ESP32 is already connected after the call to WiFi.begin(); otherwise
   * the connection will continue asynchronously.
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

  /**
   * Returns true if the manager has exhausted its retry budget without
   * establishing a link. The caller can use this to drive UI feedback
   * (e.g. an error LED effect).
   */
  bool hasConnectionFailed() const { return _reportedFailure; }

  /**
   * Returns true while an initial or recovery connection attempt is in
   * progress. This allows the caller to differentiate between an active
   * attempt and an idle, disconnected state.
   */
  bool isConnecting() const;

private:
  enum class ConnectionState { Idle, Connecting, Connected };

  ConnectionState _state = ConnectionState::Idle;
  unsigned long _lastCheckMillis = 0;
  uint8_t _retryCount = 0;
  bool _reportedFailure = false;
};
