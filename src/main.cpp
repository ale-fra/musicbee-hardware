/*
 * main.cpp
 *
 * Entry point for the NFC jukebox firmware with enhanced debugging and mDNS support
 */

#include <Arduino.h>
#include <ESPmDNS.h>

#include "Config.h"
#include "WifiManager.h"
#include "RfidReader.h"
#include "BackendClient.h"
#include "Led.h"

static WifiManager wifi;
static RfidReader rfid;
static BackendClient backend;
static Led statusLed;

// Variables to handle debouncing of repeated card reads
static String lastUid = "";
static unsigned long lastReadTime = 0;
static unsigned long lastDebugTime = 0;
static bool mdnsStarted = false;
static bool wifiPreviouslyConnected = false;

static void initializeMdns() {
  if (mdnsStarted) {
    return;
  }

  Serial.println("Initializing mDNS...");
  if (MDNS.begin("nfc-jukebox")) {
    mdnsStarted = true;
    Serial.println("mDNS responder started");
    Serial.println("ESP32 is now discoverable as nfc-jukebox.local");

    // Test mDNS resolution of backend host if it's a .local domain
    String backendHost = String(BACKEND_HOST);
    if (backendHost.endsWith(".local")) {
      Serial.printf("Testing mDNS resolution for backend: %s\n", BACKEND_HOST);

      String hostname = backendHost;
      hostname.replace(".local", "");
      IPAddress resolvedIP = MDNS.queryHost(hostname);

      if (resolvedIP == IPAddress(0, 0, 0, 0)) {
        Serial.printf("[WARNING] Could not resolve %s via mDNS\n", BACKEND_HOST);
        Serial.println("Make sure your backend server is running and mDNS is enabled");
      } else {
        Serial.printf("[SUCCESS] %s resolved to %s\n", BACKEND_HOST, resolvedIP.toString().c_str());
      }
    }
  } else {
    Serial.println("Error starting mDNS responder");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial time to initialize
  Serial.println();
  Serial.println("=================================");
  Serial.println("Jukebox NFC starting...");
  Serial.println("=================================");

  // Initialise RGB LED
  Serial.println("Initializing RGB LED...");
  statusLed.begin(LED_RED_PIN, LED_GREEN_PIN, LED_BLUE_PIN, LED_COMMON_ANODE);
  Serial.printf("RGB LED initialized - R:%d G:%d B:%d\n", LED_RED_PIN, LED_GREEN_PIN, LED_BLUE_PIN);

  // Connect to Wi-Fi
  Serial.println("Starting Wi-Fi connection...");
  statusLed.showWifiStatus(false, true); // Show connecting
  if (!wifi.begin()) {
    Serial.println("Initial Wi-Fi connection failed. The device will continue retrying in the background.");
    statusLed.showWifiStatus(false, false); // Show failed
  } else {
    Serial.println("Wi-Fi connected successfully!");
    statusLed.showWifiStatus(true, false); // Show connected

    initializeMdns();
  }

  wifiPreviouslyConnected = wifi.isConnected();

  // Initialise NFC reader
  Serial.println("Initializing NFC reader...");
  rfid.begin();
  Serial.println("NFC reader initialized");

  Serial.println("=================================");
  Serial.println("Setup complete. Ready to scan cards.");
  Serial.println("Place an NFC card near the reader...");
  Serial.println("=================================\n");
  
  // Test blink to show LED is working
  Serial.println("Testing RGB LED...");
  statusLed.setRed();
  delay(300);
  statusLed.setGreen(); 
  delay(300);
  statusLed.setBlue();
  delay(300);
  statusLed.off();
}

void loop() {
  // Maintain Wi-Fi connection
  wifi.loop();

  bool isConnected = wifi.isConnected();
  if (isConnected && !wifiPreviouslyConnected) {
    initializeMdns();
  } else if (!isConnected && wifiPreviouslyConnected) {
    mdnsStarted = false;
  }
  wifiPreviouslyConnected = isConnected;

  // Print periodic status (every 10 seconds)
  unsigned long now = millis();
  if (now - lastDebugTime > 10000) {
    lastDebugTime = now;
    Serial.printf("[DEBUG] Still running... WiFi: %s\n", 
                  wifi.isConnected() ? "Connected" : "Disconnected");
  }

  // Try to read a card
  String uid;
  bool cardRead = rfid.readCard(uid);
  
  if (cardRead) {
    Serial.println("*** CARD DETECTED ***");
    Serial.printf("Raw UID: %s (length: %d)\n", uid.c_str(), uid.length());
    
    // Debounce: ignore if same UID as last within debounce window
    if (uid == lastUid && (now - lastReadTime) < CARD_DEBOUNCE_MS) {
      Serial.printf("[DEBOUNCE] Ignoring repeated read (last read %lu ms ago)\n", 
                    now - lastReadTime);
      return;
    }
    
    lastUid = uid;
    lastReadTime = now;
    Serial.printf("Card accepted: UID=%s\n", uid.c_str());

    // Only send if we have Wi-Fi; otherwise blink error and skip
    if (!wifi.isConnected()) {
      Serial.println("[ERROR] Not connected to Wi-Fi. Skipping backend request.");
      statusLed.blinkError();
      return;
    }

    Serial.println("Sending request to backend...");
    bool ok = backend.postPlay(uid);
    if (ok) {
      Serial.println("[SUCCESS] Backend request successful");
      statusLed.blinkSuccess();
    } else {
      Serial.println("[ERROR] Backend request failed");
      statusLed.blinkError();
    }
    Serial.println("*** END CARD PROCESSING ***\n");
  }
  
  // Small delay to prevent overwhelming the system
  delay(100);
}