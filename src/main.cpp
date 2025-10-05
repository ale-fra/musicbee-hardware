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

namespace {
constexpr float kTailPrimaryFactor = 0.5f;
constexpr float kTailSecondaryFactor = 0.2f;
constexpr unsigned long kWifiCometIntervalMs = 40;
constexpr unsigned long kCardSpinnerIntervalMs = 40;
constexpr unsigned long kSuccessFlashMs = 80;
constexpr unsigned long kSuccessSpinIntervalMs = 28;
constexpr unsigned long kSuccessHoldMs = 350;
constexpr unsigned long kErrorStrobeOnMs = 120;
constexpr unsigned long kErrorStrobeOffMs = 80;
constexpr unsigned long kErrorCometIntervalMs = 45;
constexpr unsigned long kErrorFadeDurationMs = 300;
}

enum class VisualState {
  Idle,
  WifiConnecting,
  CardScanning,
  SuccessFlash,
  SuccessSpin,
  SuccessHold,
  ErrorStrobeOn1,
  ErrorStrobeOff1,
  ErrorStrobeOn2,
  ErrorStrobeOff2,
  ErrorComet,
  ErrorFade
};

static VisualState baseVisualState = VisualState::WifiConnecting;
static VisualState currentVisualState = VisualState::WifiConnecting;
static unsigned long visualStateChangedAt = 0;
static bool visualStateInitialized = false;

static const char *visualStateName(VisualState state) {
  switch (state) {
    case VisualState::Idle:
      return "Idle";
    case VisualState::WifiConnecting:
      return "WifiConnecting";
    case VisualState::CardScanning:
      return "CardScanning";
    case VisualState::SuccessFlash:
      return "SuccessFlash";
    case VisualState::SuccessSpin:
      return "SuccessSpin";
    case VisualState::SuccessHold:
      return "SuccessHold";
    case VisualState::ErrorStrobeOn1:
      return "ErrorStrobeOn1";
    case VisualState::ErrorStrobeOff1:
      return "ErrorStrobeOff1";
    case VisualState::ErrorStrobeOn2:
      return "ErrorStrobeOn2";
    case VisualState::ErrorStrobeOff2:
      return "ErrorStrobeOff2";
    case VisualState::ErrorComet:
      return "ErrorComet";
    case VisualState::ErrorFade:
      return "ErrorFade";
  }
  return "Unknown";
}

static bool isCardFlowState(VisualState state) {
  switch (state) {
    case VisualState::CardScanning:
    case VisualState::SuccessFlash:
    case VisualState::SuccessSpin:
    case VisualState::SuccessHold:
    case VisualState::ErrorStrobeOn1:
    case VisualState::ErrorStrobeOff1:
    case VisualState::ErrorStrobeOn2:
    case VisualState::ErrorStrobeOff2:
    case VisualState::ErrorComet:
    case VisualState::ErrorFade:
      return true;
    case VisualState::Idle:
    case VisualState::WifiConnecting:
      return false;
  }
  return false;
}

static unsigned long successSpinDurationMs() {
  uint16_t count = effects.ledCount();
  return static_cast<unsigned long>(count) * kSuccessSpinIntervalMs;
}

static unsigned long errorCometDurationMs() {
  uint16_t count = effects.ledCount();
  return static_cast<unsigned long>(count) * kErrorCometIntervalMs;
}

static void applyVisualState(VisualState state, unsigned long now) {
  switch (state) {
    case VisualState::Idle:
      effects.showSolidColor(0, 0, 0, now);
      break;
    case VisualState::WifiConnecting:
      effects.showComet(0, 0, 255,
                        kTailPrimaryFactor, kTailSecondaryFactor,
                        CometEffect::Direction::Clockwise,
                        kWifiCometIntervalMs, now);
      break;
    case VisualState::CardScanning:
      effects.showComet(255, 255, 255,
                        kTailPrimaryFactor, kTailSecondaryFactor,
                        CometEffect::Direction::Clockwise,
                        kCardSpinnerIntervalMs, now);
      break;
    case VisualState::SuccessFlash:
      effects.showSolidColor(255, 255, 255, now);
      break;
    case VisualState::SuccessSpin:
      effects.showComet(0, 255, 0,
                        kTailPrimaryFactor, kTailSecondaryFactor,
                        CometEffect::Direction::Clockwise,
                        kSuccessSpinIntervalMs, now);
      break;
    case VisualState::SuccessHold:
      effects.showSolidColor(0, 255, 0, now);
      break;
    case VisualState::ErrorStrobeOn1:
    case VisualState::ErrorStrobeOn2:
      effects.showSolidColor(255, 0, 0, now);
      break;
    case VisualState::ErrorStrobeOff1:
    case VisualState::ErrorStrobeOff2:
      effects.showSolidColor(0, 0, 0, now);
      break;
    case VisualState::ErrorComet:
      effects.showComet(255, 0, 0,
                        kTailPrimaryFactor, kTailSecondaryFactor,
                        CometEffect::Direction::CounterClockwise,
                        kErrorCometIntervalMs, now);
      break;
    case VisualState::ErrorFade:
      effects.showFade(255, 0, 0, 0, 0, 0, kErrorFadeDurationMs, now);
      break;
  }
}

static void setVisualState(VisualState state, unsigned long now) {
  if (visualStateInitialized && currentVisualState == state) {
    return;
  }
  if (currentVisualState != state) {
    Serial.printf("[State] Transitioning from %s to %s at %lums\n",
                  visualStateName(currentVisualState), visualStateName(state), now);
  }
  currentVisualState = state;
  visualStateChangedAt = now;
  visualStateInitialized = true;
  applyVisualState(state, now);
}

static void updateWifiVisualState(bool isConnected, unsigned long now) {
  VisualState desiredBase = isConnected ? VisualState::Idle : VisualState::WifiConnecting;
  if (baseVisualState != desiredBase) {
    Serial.printf("[State] Base visual state set to %s at %lums\n",
                  visualStateName(desiredBase), now);
    baseVisualState = desiredBase;
  }

  if (!isConnected) {
    if (currentVisualState != VisualState::WifiConnecting) {
      Serial.printf("[State] Wi-Fi disconnected, forcing WifiConnecting at %lums\n", now);
      setVisualState(VisualState::WifiConnecting, now);
    }
  } else if (!isCardFlowState(currentVisualState)) {
    setVisualState(VisualState::Idle, now);
  }
}

static void updateTimedTransitions(unsigned long now) {
  switch (currentVisualState) {
    case VisualState::SuccessFlash:
      if (now - visualStateChangedAt >= kSuccessFlashMs) {
        setVisualState(VisualState::SuccessSpin, now);
      }
      break;
    case VisualState::SuccessSpin:
      if (now - visualStateChangedAt >= successSpinDurationMs()) {
        setVisualState(VisualState::SuccessHold, now);
      }
      break;
    case VisualState::SuccessHold:
      if (now - visualStateChangedAt >= kSuccessHoldMs) {
        setVisualState(baseVisualState, now);
      }
      break;
    case VisualState::ErrorStrobeOn1:
      if (now - visualStateChangedAt >= kErrorStrobeOnMs) {
        setVisualState(VisualState::ErrorStrobeOff1, now);
      }
      break;
    case VisualState::ErrorStrobeOff1:
      if (now - visualStateChangedAt >= kErrorStrobeOffMs) {
        setVisualState(VisualState::ErrorStrobeOn2, now);
      }
      break;
    case VisualState::ErrorStrobeOn2:
      if (now - visualStateChangedAt >= kErrorStrobeOnMs) {
        setVisualState(VisualState::ErrorStrobeOff2, now);
      }
      break;
    case VisualState::ErrorStrobeOff2:
      if (now - visualStateChangedAt >= kErrorStrobeOffMs) {
        setVisualState(VisualState::ErrorComet, now);
      }
      break;
    case VisualState::ErrorComet:
      if (now - visualStateChangedAt >= errorCometDurationMs()) {
        setVisualState(VisualState::ErrorFade, now);
      }
      break;
    case VisualState::ErrorFade:
      if (now - visualStateChangedAt >= kErrorFadeDurationMs) {
        setVisualState(baseVisualState, now);
      }
      break;
    case VisualState::Idle:
    case VisualState::WifiConnecting:
    case VisualState::CardScanning:
      break;
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
  setVisualState(VisualState::WifiConnecting, now);

  // Connect to Wi-Fi
  Serial.println("Starting Wi-Fi connection...");
  if (!wifi.begin()) {
    Serial.println("Initial Wi-Fi connection failed. The device will continue retrying in the background.");
    updateWifiVisualState(false, millis());
  } else {
    Serial.println("Wi-Fi connected successfully!");
    unsigned long connectedNow = millis();
    updateWifiVisualState(true, connectedNow);
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
  
}

void loop() {
  // Maintain Wi-Fi connection
  wifi.loop();

  unsigned long now = millis();

  bool isConnected = wifi.isConnected();
  if (isConnected && !wifiPreviouslyConnected) {
    initializeMdns();
  } else if (!isConnected && wifiPreviouslyConnected) {
    mdnsStarted = false;
  }
  updateWifiVisualState(isConnected, now);
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
        updateWifiVisualState(false, millis());
      } else {
        setVisualState(VisualState::CardScanning, now);
        Serial.println("Sending request to backend...");
        bool ok = backend.postPlay(uid);
        now = millis();
        if (ok) {
          Serial.println("[SUCCESS] Backend request successful");
          setVisualState(VisualState::SuccessFlash, now);
        } else {
          Serial.println("[ERROR] Backend request failed");
          setVisualState(VisualState::ErrorStrobeOn1, now);
        }
      }
      Serial.println("*** END CARD PROCESSING ***\n");
    }
  }

  const unsigned long effectsNow = millis();
  updateTimedTransitions(effectsNow);
  effects.update(effectsNow);
}
