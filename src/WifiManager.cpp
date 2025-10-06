/*
 * WifiManager.cpp
 *
 * Implements the WifiManager class declared in WifiManager.h. It
 * encapsulates connecting to Wi‑Fi and handling reconnection logic.
 */

#include "WifiManager.h"

namespace {
constexpr bool hasRetryLimit() {
  return MAX_WIFI_RETRIES > 0;
}
}

bool WifiManager::begin() {
  WiFi.mode(WIFI_STA);
  Serial.println("Connecting to Wi-Fi...");

  _attemptsThisCycle = 0;
  _state = ConnectionState::Idle;
  _stateChangedAt = millis();
  _hasLoggedConnected = false;

  startConnectionAttempt(_stateChangedAt);
  return WiFi.status() == WL_CONNECTED;
}

void WifiManager::loop() {
  unsigned long now = millis();
  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    if (!_hasLoggedConnected) {
      Serial.printf("[WiFi] Connected to %s, IP: %s\n",
                    WIFI_SSID,
                    WiFi.localIP().toString().c_str());
      _hasLoggedConnected = true;
    }
    _state = ConnectionState::Idle;
    _stateChangedAt = now;
    _attemptsThisCycle = 0;
    return;
  }

  _hasLoggedConnected = false;

  switch (_state) {
    case ConnectionState::Idle:
      if (_attemptsThisCycle == 0 || now - _stateChangedAt >= WIFI_RETRY_DELAY_MS) {
        if (!hasRetryLimit() || _attemptsThisCycle < MAX_WIFI_RETRIES) {
          startConnectionAttempt(now);
        } else {
          Serial.println("[WiFi] Maximum retries reached. Backing off before retrying.");
          _state = ConnectionState::Cooldown;
          _stateChangedAt = now;
        }
      }
      break;

    case ConnectionState::Connecting:
      if (now - _stateChangedAt >= WIFI_RETRY_DELAY_MS) {
        if (!hasRetryLimit() || _attemptsThisCycle < MAX_WIFI_RETRIES) {
          Serial.println("[WiFi] Connection timed out, retrying...");
          startConnectionAttempt(now);
        } else {
          Serial.println("[WiFi] Maximum retries reached. Backing off before retrying.");
          _state = ConnectionState::Cooldown;
          _stateChangedAt = now;
        }
      }
      break;

    case ConnectionState::Cooldown:
      if (now - _stateChangedAt >= WIFI_RETRY_DELAY_MS) {
        _attemptsThisCycle = 0;
        startConnectionAttempt(now);
      }
      break;
  }
}

bool WifiManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

void WifiManager::startConnectionAttempt(unsigned long now) {
  if (_attemptsThisCycle > 0) {
    WiFi.disconnect();
  }

  uint8_t attemptNumber = _attemptsThisCycle + 1;
  if (hasRetryLimit()) {
    Serial.printf("[WiFi] Attempt %u/%u to connect...\n", attemptNumber, MAX_WIFI_RETRIES);
  } else {
    Serial.printf("[WiFi] Attempt %u to connect...\n", attemptNumber);
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  _state = ConnectionState::Connecting;
  _stateChangedAt = now;
  _attemptsThisCycle++;
}
