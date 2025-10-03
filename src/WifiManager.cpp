/*
 * WifiManager.cpp
 *
 * Implements the WifiManager class declared in WifiManager.h. It
 * encapsulates connecting to Wi‑Fi and handling reconnection logic.
 */

#include "WifiManager.h"

bool WifiManager::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi...");

  _state = (WiFi.status() == WL_CONNECTED) ? ConnectionState::Connected : ConnectionState::Connecting;
  _retryCount = 0;
  _reportedFailure = false;
  _lastCheckMillis = millis();

  if (_state == ConnectionState::Connected) {
    Serial.printf("Connected to %s, IP: %s\n", WIFI_SSID, WiFi.localIP().toString().c_str());
    return true;
  }

  return false;
}

void WifiManager::loop() {
  unsigned long now = millis();
  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    if (_state != ConnectionState::Connected) {
      Serial.printf("Connected to %s, IP: %s\n", WIFI_SSID, WiFi.localIP().toString().c_str());
    }
    _state = ConnectionState::Connected;
    _retryCount = 0;
    _reportedFailure = false;
    return;
  }

  if (_state == ConnectionState::Connected) {
    Serial.println("Wi-Fi disconnected, attempting reconnect...");
    _state = ConnectionState::Connecting;
    _retryCount = 0;
  }

  if (status == WL_IDLE_STATUS) {
    // A connection attempt is already underway; wait for it to
    // complete without restarting the state machine.
    return;
  }

  if (now - _lastCheckMillis < WIFI_RETRY_DELAY_MS) {
    return;
  }
  _lastCheckMillis = now;

  if (_state != ConnectionState::Connecting) {
    _state = ConnectionState::Connecting;
    _retryCount = 0;
  }

  if (_retryCount < MAX_WIFI_RETRIES) {
    ++_retryCount;
    Serial.println("Retrying Wi-Fi connection...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  } else {
    if (!_reportedFailure) {
      Serial.println("Failed to connect to Wi-Fi");
      _reportedFailure = true;
    }
    WiFi.reconnect();
  }
}

bool WifiManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool WifiManager::isConnecting() const {
  return _state == ConnectionState::Connecting;
}
