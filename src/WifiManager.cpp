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

  Serial.print("Connecting to Wi-Fi...\n");
  uint8_t retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < MAX_WIFI_RETRIES) {
    delay(WIFI_RETRY_DELAY_MS);
    Serial.print('.');
    retries++;
  }

  bool connected = (WiFi.status() == WL_CONNECTED);
  if (connected) {
    Serial.printf("\nConnected to %s, IP: %s\n", WIFI_SSID, WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi");
  }
  return connected;
}

void WifiManager::loop() {
  // Only check every 5 seconds to reduce overhead
  if (millis() - _lastCheckMillis < 5000UL) {
    return;
  }
  _lastCheckMillis = millis();

  wl_status_t status = WiFi.status();
  if (status != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected, attempting reconnect...");
    WiFi.disconnect();
    WiFi.reconnect();
  }
}

bool WifiManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}