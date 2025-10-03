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
#include "EffectManager.h"

static WifiManager wifi;
static RfidReader rfid;
static BackendClient backend;
static EffectManager effects(LED_DATA_PIN, LED_COUNT_DEFAULT, LED_BRIGHTNESS_DEFAULT);

enum class VisualState {
  BootWarmWhite,
  WifiConnecting,
  WifiConnected,
  WifiError,
  CardDetected,
  BackendSuccess,
  BackendError
};

static VisualState baseVisualState = VisualState::BootWarmWhite;
static VisualState currentVisualState = VisualState::BootWarmWhite;
static VisualState resumeVisualState = VisualState::BootWarmWhite;
static unsigned long visualStateChangedAt = 0;
static constexpr unsigned long TRANSIENT_EFFECT_DURATION_MS = 2500;
static constexpr unsigned long BOOT_WINDOW_DURATION_MS = 10000;

static unsigned long startupMillis = 0;
static bool bootWindowElapsed = false;
static VisualState desiredWifiState = VisualState::WifiConnecting;

static bool isTransientState(VisualState state) {
  return state == VisualState::CardDetected || state == VisualState::BackendSuccess ||
         state == VisualState::BackendError;
}

static void applyVisualState(VisualState state, unsigned long now) {
  switch (state) {
    case VisualState::BootWarmWhite:
      effects.breathingEffect().setPeriod(1800);
      effects.showBreathing(255, 244, 229, now);
      break;
    case VisualState::WifiConnecting:
      effects.breathingEffect().setPeriod(1500);
      effects.showBreathing(0, 0, 255, now);
      break;
    case VisualState::WifiConnected:
      effects.showSolidColor(0, 0, 0, now);
      break;
    case VisualState::WifiError:
      effects.showSolidColor(255, 0, 0, now);
      break;
    case VisualState::CardDetected:
      effects.showBlink(0, 255, 0, 120, 80, 3, now);
      break;
    case VisualState::BackendSuccess:
      effects.snakeEffect().setInterval(90);
      effects.showSnake(0, 255, 0, now);
      break;
    case VisualState::BackendError:
      effects.breathingEffect().setPeriod(1200);
      effects.showBreathing(255, 0, 0, now);
      break;
  }
}

static void setVisualState(VisualState state, unsigned long now) {
  if (isTransientState(state)) {
    resumeVisualState = baseVisualState;
  } else {
    resumeVisualState = state;
  }
  currentVisualState = state;
  visualStateChangedAt = now;
  applyVisualState(state, now);
}

static void setBaseVisualState(VisualState state, unsigned long now) {
  baseVisualState = state;
  resumeVisualState = state;
  if (!isTransientState(currentVisualState) || currentVisualState == state) {
    setVisualState(state, now);
  }
}

static void refreshVisualState(unsigned long now) {
  if (isTransientState(currentVisualState) &&
      (effects.isActiveEffectFinished() ||
       now - visualStateChangedAt >= TRANSIENT_EFFECT_DURATION_MS)) {
    setVisualState(resumeVisualState, now);
  } else if (!isTransientState(currentVisualState) && currentVisualState != baseVisualState) {
    setVisualState(baseVisualState, now);
  }
}

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

  Serial.println("Initializing LED strip...");
  unsigned long now = millis();
  effects.begin(now);
  startupMillis = now;
  bootWindowElapsed = false;
  desiredWifiState = VisualState::WifiConnecting;
  setBaseVisualState(VisualState::BootWarmWhite, now);

  // Connect to Wi-Fi
  Serial.println("Starting Wi-Fi connection...");
  bool connectedImmediately = wifi.begin();
  if (connectedImmediately) {
    Serial.println("Wi-Fi connected successfully!");
    desiredWifiState = VisualState::WifiConnected;
    initializeMdns();
  } else {
    Serial.println("Wi-Fi connection attempt started. Waiting for link...");
    desiredWifiState = VisualState::WifiConnecting;
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
  
}

void loop() {
  // Maintain Wi-Fi connection
  wifi.loop();

  unsigned long now = millis();

  bool isConnected = wifi.isConnected();
  if (wifi.hasConnectionFailed()) {
    desiredWifiState = VisualState::WifiError;
  } else if (isConnected) {
    desiredWifiState = VisualState::WifiConnected;
  } else {
    desiredWifiState = VisualState::WifiConnecting;
  }

  if (isConnected && !wifiPreviouslyConnected) {
    initializeMdns();
  } else if (!isConnected && wifiPreviouslyConnected) {
    mdnsStarted = false;
  }

  if (!bootWindowElapsed) {
    unsigned long uptime = now - startupMillis;
    if (uptime >= BOOT_WINDOW_DURATION_MS) {
      bootWindowElapsed = true;
      setBaseVisualState(desiredWifiState, now);
    }
  } else if (baseVisualState != desiredWifiState) {
    setBaseVisualState(desiredWifiState, now);
  }

  wifiPreviouslyConnected = isConnected;

  // Print periodic status (every 10 seconds)
  if (now - lastDebugTime > 10000) {
    lastDebugTime = now;
    Serial.printf("[DEBUG] Still running... WiFi: %s\n",
                  isConnected ? "Connected" : "Disconnected");
  }

  // Try to read a card
  String uid;
  bool cardRead = rfid.readCard(uid);
  
  if (cardRead) {
    Serial.println("*** CARD DETECTED ***");
    Serial.printf("Raw UID: %s (length: %d)\n", uid.c_str(), uid.length());

    now = millis();
    setVisualState(VisualState::CardDetected, now);

    bool isDuplicate = uid == lastUid && (now - lastReadTime) < CARD_DEBOUNCE_MS;
    if (isDuplicate) {
      Serial.printf("[DEBOUNCE] Ignoring repeated read (last read %lu ms ago)\n",
                    now - lastReadTime);
    } else {
      lastUid = uid;
      lastReadTime = now;
      Serial.printf("Card accepted: UID=%s\n", uid.c_str());

      if (!wifi.isConnected()) {
        Serial.println("[ERROR] Not connected to Wi-Fi. Skipping backend request.");
        setVisualState(VisualState::BackendError, millis());
        now = millis();
      } else {
        Serial.println("Sending request to backend...");
        bool ok = backend.postPlay(uid);
        now = millis();
        if (ok) {
          Serial.println("[SUCCESS] Backend request successful");
          setVisualState(VisualState::BackendSuccess, now);
        } else {
          Serial.println("[ERROR] Backend request failed");
          setVisualState(VisualState::BackendError, now);
        }
      }
      Serial.println("*** END CARD PROCESSING ***\n");
    }
  }

  now = millis();
  refreshVisualState(now);
  effects.update(now);
}
