/*
 * main.cpp
 *
 * Entry point for the NFC jukebox firmware with enhanced debugging and mDNS support
 */

#include <Arduino.h>
#include <ESPmDNS.h>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Config.h"
#include "WifiManager.h"
#include "RfidReader.h"
#include "BackendClient.h"
#include "EffectManager.h"

#if ENABLE_DEBUG_ACTIONS
#  include "DebugActionServer.h"
#endif

static WifiManager wifi;
static RfidReader rfid;
static BackendClient backend;
static EffectManager effects(LED_DATA_PIN, LED_COUNT_DEFAULT, LED_BRIGHTNESS_DEFAULT);

namespace {
constexpr float kTailPrimaryFactor = 0.5f;
constexpr float kTailSecondaryFactor = 0.2f;
constexpr unsigned long kWifiCometIntervalMs = 40;
constexpr unsigned long kCardSpinnerIntervalMs = 40;
constexpr unsigned long kSuccessSpinIntervalMs = 28;
constexpr unsigned long kErrorFadeDurationMs = 300;
}

enum class VisualState {
  Idle,
  WifiConnecting,
  WifiConnected,
  WifiError,
  CardDetected,
  CardScanning,
  BackendSuccess,
  BackendError,
  MdnsResolving,
  MdnsSuccess,
  MdnsError
};

static VisualState baseVisualState = VisualState::WifiConnecting;
static VisualState currentVisualState = VisualState::WifiConnecting;
static unsigned long visualStateChangedAt = 0;
static constexpr unsigned long TRANSIENT_EFFECT_DURATION_MS = 2500;
static bool pendingBackendRequest = false;
static bool visualStateInitialized = false;
static bool wifiPreviouslyConnected = false;
static constexpr UBaseType_t MDNS_QUERY_TASK_PRIORITY = 1;
static constexpr uint16_t MDNS_QUERY_TASK_STACK_SIZE = 4096;

enum class MdnsQueryState {
  Idle,
  NotRequired,
  Pending,
  Success,
  Failure
};

struct MdnsQueryUpdate {
  MdnsQueryState state = MdnsQueryState::Idle;
  String hostname;
  IPAddress resolvedIp;
  unsigned long startedAt = 0;
  unsigned long finishedAt = 0;
};

static volatile MdnsQueryState mdnsQueryState = MdnsQueryState::Idle;
static MdnsQueryState lastReportedMdnsState = MdnsQueryState::Idle;
static TaskHandle_t mdnsQueryTaskHandle = nullptr;
static String mdnsQueryHostname;
static IPAddress mdnsResolvedIp;
static unsigned long mdnsQueryFinishedAt = 0;
static unsigned long mdnsQueryStartedAt = 0;

static bool scheduleMdnsQueryTask(const String &hostname, unsigned long now);
static void mdnsQueryTask(void *param);
static bool isCardFlowState(VisualState state);
static void setVisualState(VisualState state, unsigned long now);

static void resetMdnsQueryState() {
  if (mdnsQueryTaskHandle == nullptr) {
    mdnsQueryHostname = "";
  }
  mdnsResolvedIp = IPAddress();
  mdnsQueryFinishedAt = 0;
  mdnsQueryStartedAt = 0;
  mdnsQueryState = MdnsQueryState::Idle;
  lastReportedMdnsState = MdnsQueryState::Idle;
}

static void showMdnsVisualState(VisualState state, unsigned long now) {
  if (!isCardFlowState(currentVisualState)) {
    setVisualState(state, now);
  }
}

static bool fetchMdnsQueryUpdate(MdnsQueryUpdate &out) {
  MdnsQueryState state = mdnsQueryState;
  if (state == lastReportedMdnsState) {
    return false;
  }

  lastReportedMdnsState = state;
  out.state = state;
  out.hostname = mdnsQueryHostname;
  out.resolvedIp = mdnsResolvedIp;
  out.startedAt = mdnsQueryStartedAt;
  out.finishedAt = mdnsQueryFinishedAt;
  return true;
}

static void applyMdnsUpdateFeedback(const MdnsQueryUpdate &update, unsigned long now) {
  switch (update.state) {
    case MdnsQueryState::Pending:
      Serial.printf("[mDNS] Resolving %s.local asynchronously...\n",
                    update.hostname.c_str());
      showMdnsVisualState(VisualState::MdnsResolving, now);
      break;
    case MdnsQueryState::Success:
      Serial.printf("[mDNS] %s.local resolved to %s\n",
                    update.hostname.c_str(), update.resolvedIp.toString().c_str());
      if (update.startedAt != 0 && update.finishedAt >= update.startedAt) {
        Serial.printf("[mDNS] Query completed in %lums\n",
                      update.finishedAt - update.startedAt);
      }
      showMdnsVisualState(VisualState::MdnsSuccess, now);
      break;
    case MdnsQueryState::Failure:
      Serial.printf("[WARNING] Could not resolve %s.local via mDNS\n",
                    update.hostname.c_str());
      Serial.println("Make sure your backend server is running and mDNS is enabled");
      if (update.startedAt != 0 && update.finishedAt >= update.startedAt) {
        Serial.printf("[mDNS] Query failed after %lums\n",
                      update.finishedAt - update.startedAt);
      }
      showMdnsVisualState(VisualState::MdnsError, now);
      break;
    case MdnsQueryState::NotRequired:
    case MdnsQueryState::Idle:
      break;
  }
}

static void mdnsQueryTask(void *param) {
  std::unique_ptr<char, void (*)(void *)> hostnamePtr(static_cast<char *>(param), free);
  const char *hostnameCStr = hostnamePtr.get();
  IPAddress resolved = MDNS.queryHost(hostnameCStr ? hostnameCStr : "");

  mdnsResolvedIp = resolved;
  mdnsQueryFinishedAt = millis();
  if (resolved == IPAddress(0, 0, 0, 0)) {
    mdnsQueryState = MdnsQueryState::Failure;
  } else {
    mdnsQueryState = MdnsQueryState::Success;
  }
  mdnsQueryTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

static bool isCardFlowState(VisualState state) {
  return state == VisualState::CardDetected || state == VisualState::BackendSuccess ||
         state == VisualState::BackendError;
}

static bool isTransientState(VisualState state) {
  return isCardFlowState(state) || state == VisualState::MdnsSuccess ||
         state == VisualState::MdnsError;
}

static const char *visualStateName(VisualState state) {
  switch (state) {
    case VisualState::Idle:
      return "Idle";
    case VisualState::WifiConnecting:
      return "WifiConnecting";
    case VisualState::WifiConnected:
      return "WifiConnected";
    case VisualState::WifiError:
      return "WifiError";
    case VisualState::CardDetected:
      return "CardDetected";
    case VisualState::CardScanning:
      return "CardScanning";
    case VisualState::BackendSuccess:
      return "BackendSuccess";
    case VisualState::BackendError:
      return "BackendError";
    case VisualState::MdnsResolving:
      return "MdnsResolving";
    case VisualState::MdnsSuccess:
      return "MdnsSuccess";
    case VisualState::MdnsError:
      return "MdnsError";
  }
  return "Unknown";
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
    case VisualState::WifiConnected:
      effects.breathingEffect().setPeriod(1500);
      effects.showBreathing(0, 128, 255, now);
      break;
    case VisualState::WifiError:
      effects.showFade(255, 0, 0, 80, 0, 0, kErrorFadeDurationMs, now);
      break;
    case VisualState::CardDetected:
      effects.showSolidColor(255, 255, 255, now);
      break;
    case VisualState::CardScanning:
      effects.showComet(255, 255, 255,
                        kTailPrimaryFactor, kTailSecondaryFactor,
                        CometEffect::Direction::Clockwise,
                        kCardSpinnerIntervalMs, now);
      break;
    case VisualState::BackendSuccess:
      effects.snakeEffect().setInterval(kSuccessSpinIntervalMs);
      effects.showSnake(0, 255, 0, now);
      break;
    case VisualState::BackendError:
      effects.showFade(255, 0, 0, 0, 0, 0, kErrorFadeDurationMs, now);
      break;
    case VisualState::MdnsResolving:
      effects.showComet(0, 128, 255,
                        kTailPrimaryFactor, kTailSecondaryFactor,
                        CometEffect::Direction::Clockwise,
                        kWifiCometIntervalMs, now);
      break;
    case VisualState::MdnsSuccess:
      effects.showSolidColor(0, 64, 0, now);
      break;
    case VisualState::MdnsError:
      effects.showFade(255, 32, 32, 0, 0, 0, kErrorFadeDurationMs, now);
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

static void setBaseVisualState(VisualState state, unsigned long now) {
  baseVisualState = state;
  bool shouldApply = !isTransientState(currentVisualState) || currentVisualState == state;
  if (currentVisualState == VisualState::CardScanning && pendingBackendRequest) {
    shouldApply = false;
  }
  if (shouldApply) {
    setVisualState(state, now);
  }
}

static void refreshVisualState(unsigned long now) {
  if (isTransientState(currentVisualState) &&
      now - visualStateChangedAt >= TRANSIENT_EFFECT_DURATION_MS) {
    if (pendingBackendRequest) {
      setVisualState(VisualState::CardScanning, now);
    } else {
      setVisualState(baseVisualState, now);
    }
  }
}

static void updateWifiVisualState(bool isConnected, unsigned long now) {
  VisualState target = VisualState::WifiConnecting;
  if (isConnected) {
    target = VisualState::WifiConnected;
  } else if (visualStateInitialized && wifiPreviouslyConnected) {
    target = VisualState::WifiError;
  }
  setBaseVisualState(target, now);
}

// Variables to handle debouncing of repeated card reads
static String lastUid = "";
static unsigned long lastReadTime = 0;
static unsigned long lastDebugTime = 0;
static bool mdnsStarted = false;

#if ENABLE_DEBUG_ACTIONS
static DebugActionServer debugServer(DEBUG_SERVER_PORT);
#endif

enum class CardProcessResult {
  DuplicateIgnored,
  BackendSkipped,
  BackendSuccess,
  BackendFailure,
  WifiDisconnected
};

static CardProcessResult processCardUid(const String &uid, unsigned long now,
                                       bool bypassDebounce, bool sendToBackend) {
  Serial.println("*** CARD DETECTED ***");
  Serial.printf("Raw UID: %s (length: %d)\n", uid.c_str(), uid.length());

  setVisualState(VisualState::CardDetected, now);

  bool isDuplicate = !bypassDebounce && uid == lastUid &&
                     (now - lastReadTime) < CARD_DEBOUNCE_MS;
  if (isDuplicate) {
    Serial.printf("[DEBOUNCE] Ignoring repeated read (last read %lu ms ago)\n",
                  now - lastReadTime);
    return CardProcessResult::DuplicateIgnored;
  }

  lastUid = uid;
  lastReadTime = now;
  Serial.printf("Card accepted: UID=%s\n", uid.c_str());

  if (!sendToBackend) {
    Serial.println("[DEBUG] Backend request skipped (sendToBackend=false).");
    unsigned long updatedNow = millis();
    setVisualState(VisualState::BackendSuccess, updatedNow);
    Serial.println("*** END CARD PROCESSING ***\n");
    return CardProcessResult::BackendSkipped;
  }

  if (!wifi.isConnected()) {
    Serial.println("[ERROR] Not connected to Wi-Fi. Skipping backend request.");
    unsigned long updatedNow = millis();
    setVisualState(VisualState::BackendError, updatedNow);
    Serial.println("*** END CARD PROCESSING ***\n");
    return CardProcessResult::WifiDisconnected;
  }

  Serial.println("Sending request to backend...");
  bool ok = backend.postPlay(uid);
  unsigned long updatedNow = millis();
  if (ok) {
    Serial.println("[SUCCESS] Backend request successful");
    setVisualState(VisualState::BackendSuccess, updatedNow);
    Serial.println("*** END CARD PROCESSING ***\n");
    return CardProcessResult::BackendSuccess;
  }

  Serial.println("[ERROR] Backend request failed");
  setVisualState(VisualState::BackendError, updatedNow);
  Serial.println("*** END CARD PROCESSING ***\n");
  return CardProcessResult::BackendFailure;
}

#if ENABLE_DEBUG_ACTIONS
static bool parseVisualState(const String &value, VisualState &state) {
  String normalized = value;
  normalized.toLowerCase();

  if (normalized == "wifi_connecting") {
    state = VisualState::WifiConnecting;
    return true;
  }
  if (normalized == "wifi_connected") {
    state = VisualState::WifiConnected;
    return true;
  }
  if (normalized == "wifi_error") {
    state = VisualState::WifiError;
    return true;
  }
  if (normalized == "card_detected") {
    state = VisualState::CardDetected;
    return true;
  }
  if (normalized == "backend_success") {
    state = VisualState::BackendSuccess;
    return true;
  }
  if (normalized == "backend_error") {
    state = VisualState::BackendError;
    return true;
  }
  if (normalized == "mdns_resolving") {
    state = VisualState::MdnsResolving;
    return true;
  }
  if (normalized == "mdns_success") {
    state = VisualState::MdnsSuccess;
    return true;
  }
  if (normalized == "mdns_error") {
    state = VisualState::MdnsError;
    return true;
  }
  return false;
}

static bool handleSetVisualState(JsonVariantConst payload, String &message) {
  if (!payload.is<JsonObjectConst>()) {
    message = "Payload must be a JSON object.";
    return false;
  }

  JsonObjectConst obj = payload.as<JsonObjectConst>();
  const char *stateName = obj["state"] | nullptr;
  if (stateName == nullptr) {
    message = "Missing 'state' field.";
    return false;
  }

  VisualState state;
  if (!parseVisualState(String(stateName), state)) {
    message = "Unknown state value.";
    return false;
  }

  bool applyToBase = obj["apply_to_base"] | false;
  unsigned long now = millis();
  if (applyToBase) {
    setBaseVisualState(state, now);
    message = "Base visual state updated.";
  } else {
    setVisualState(state, now);
    message = "Visual state updated.";
  }

  return true;
}

static bool handlePreviewEffect(JsonVariantConst payload, String &message) {
  if (!payload.is<JsonObjectConst>()) {
    message = "Payload must be a JSON object.";
    return false;
  }

  JsonObjectConst obj = payload.as<JsonObjectConst>();
  const char *typeName = obj["type"] | nullptr;
  if (typeName == nullptr) {
    message = "Missing 'type' field.";
    return false;
  }

  uint8_t r = obj["r"] | 0;
  uint8_t g = obj["g"] | 0;
  uint8_t b = obj["b"] | 0;
  unsigned long now = millis();

  String type = String(typeName);
  type.toLowerCase();

  if (type == "solid") {
    effects.showSolidColor(r, g, b, now);
    message = "Solid color preview displayed.";
    return true;
  }

  if (type == "breathing") {
    unsigned long period = obj["period_ms"] | 1500;
    effects.breathingEffect().setPeriod(period);
    effects.showBreathing(r, g, b, now);
    message = "Breathing effect preview displayed.";
    return true;
  }

  if (type == "snake") {
    unsigned long interval = obj["interval_ms"] | 90;
    effects.snakeEffect().setInterval(interval);
    effects.showSnake(r, g, b, now);
    message = "Snake effect preview displayed.";
    return true;
  }

  message = "Unsupported effect type.";
  return false;
}

static bool handleSimulateCard(JsonVariantConst payload, String &message) {
  if (!payload.is<JsonObjectConst>()) {
    message = "Payload must be a JSON object.";
    return false;
  }

  JsonObjectConst obj = payload.as<JsonObjectConst>();
  const char *uidValue = obj["uid"] | nullptr;
  if (uidValue == nullptr) {
    message = "Missing 'uid' field.";
    return false;
  }

  bool bypassDebounce = obj["bypass_debounce"] | false;
  bool sendToBackend = obj["send_to_backend"] | true;

  unsigned long now = millis();
  CardProcessResult result =
      processCardUid(String(uidValue), now, bypassDebounce, sendToBackend);

  switch (result) {
    case CardProcessResult::DuplicateIgnored:
      message = "Duplicate UID ignored due to debounce.";
      return false;
    case CardProcessResult::BackendSkipped:
      message = "Simulated card processed without backend call.";
      return true;
    case CardProcessResult::BackendSuccess:
      message = "Backend request completed successfully.";
      return true;
    case CardProcessResult::BackendFailure:
      message = "Backend request failed.";
      return false;
    case CardProcessResult::WifiDisconnected:
      message = "Wi-Fi is disconnected; backend request skipped.";
      return false;
  }

  message = "Unknown result.";
  return false;
}
#endif

static bool scheduleMdnsQueryTask(const String &hostname, unsigned long now) {
  if (hostname.isEmpty()) {
    return false;
  }

  if (mdnsQueryTaskHandle != nullptr) {
    Serial.println("[WARNING] Previous mDNS query still running; skipping new task");
    return false;
  }

  mdnsQueryHostname = hostname;

  size_t bufferSize = hostname.length() + 1;
  char *hostnameCopy = static_cast<char *>(malloc(bufferSize));
  if (hostnameCopy == nullptr) {
    Serial.println("[WARNING] Insufficient memory for mDNS query task");
    mdnsQueryState = MdnsQueryState::Failure;
    mdnsQueryStartedAt = 0;
    mdnsQueryFinishedAt = now;
    return false;
  }

  hostname.toCharArray(hostnameCopy, bufferSize);

  mdnsQueryState = MdnsQueryState::Pending;
  mdnsQueryStartedAt = now;
  mdnsQueryFinishedAt = 0;

  BaseType_t taskCreated = xTaskCreate(mdnsQueryTask, "MdnsQuery",
                                       MDNS_QUERY_TASK_STACK_SIZE, hostnameCopy,
                                       MDNS_QUERY_TASK_PRIORITY,
                                       &mdnsQueryTaskHandle);
  if (taskCreated != pdPASS) {
    Serial.println("[WARNING] Failed to start mDNS query task");
    free(hostnameCopy);
    mdnsQueryState = MdnsQueryState::Failure;
    mdnsQueryFinishedAt = now;
    mdnsQueryStartedAt = 0;
    mdnsQueryTaskHandle = nullptr;
    return false;
  }

  return true;
}

static void initializeMdns() {
  if (mdnsStarted) {
    return;
  }

  resetMdnsQueryState();

  Serial.println("Initializing mDNS...");
  if (!MDNS.begin("nfc-jukebox")) {
    Serial.println("Error starting mDNS responder");
    return;
  }

  mdnsStarted = true;
  Serial.println("mDNS responder started");
  Serial.println("ESP32 is now discoverable as nfc-jukebox.local");

  String backendHost = String(BACKEND_HOST);
  if (!backendHost.endsWith(".local")) {
    mdnsQueryState = MdnsQueryState::NotRequired;
    mdnsQueryStartedAt = 0;
    mdnsQueryFinishedAt = 0;
    return;
  }

  backendHost.replace(".local", "");
  unsigned long now = millis();
  if (!scheduleMdnsQueryTask(backendHost, now)) {
    mdnsQueryFinishedAt = millis();
  }
}

void setup() {
  Serial.begin(115200);
  unsigned long serialStart = millis();
  while (!Serial && (millis() - serialStart) < 1000) {
    yield();
  }
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
  bool wifiReady = wifi.begin();
  if (wifiReady) {
    Serial.println("Wi-Fi connected successfully!");
    unsigned long connectedNow = millis();
    updateWifiVisualState(true, connectedNow);
    initializeMdns();
  } else {
    Serial.println("Wi-Fi connection in progress...");
  }

  wifiPreviouslyConnected = wifiReady;

  // Initialise NFC reader
  Serial.println("Initializing NFC reader...");
  rfid.begin();
  Serial.println("NFC reader initialization in progress");

#if ENABLE_DEBUG_ACTIONS
  debugServer.registerAction({"set_visual_state",
                              "Set or override the current LED state.",
                              handleSetVisualState});
  debugServer.registerAction(
      {"preview_effect", "Preview an LED effect with custom colours.",
       handlePreviewEffect});
  debugServer.registerAction({"simulate_card",
                              "Simulate an NFC card scan with an arbitrary UID.",
                              handleSimulateCard});
  debugServer.begin();
  if (wifi.isConnected()) {
    debugServer.start();
  }
#endif

  Serial.println("=================================");
  Serial.println("Setup complete. Ready to scan cards.");
  Serial.println("Place an NFC card near the reader...");
  Serial.println("=================================\n");

}

void loop() {
  // Maintain Wi-Fi connection
  wifi.loop();

  unsigned long now = millis();

  rfid.begin();

  bool isConnected = wifi.isConnected();
  if (isConnected && !wifiPreviouslyConnected) {
    initializeMdns();
#if ENABLE_DEBUG_ACTIONS
    debugServer.start();
#endif
  } else if (!isConnected && wifiPreviouslyConnected) {
    mdnsStarted = false;
    resetMdnsQueryState();
  }
  updateWifiVisualState(isConnected, now);
  wifiPreviouslyConnected = isConnected;

  MdnsQueryUpdate mdnsUpdate;
  if (fetchMdnsQueryUpdate(mdnsUpdate)) {
    applyMdnsUpdateFeedback(mdnsUpdate, now);
  }

#if ENABLE_DEBUG_ACTIONS
  debugServer.loop();
#endif

  // Print periodic status (every 10 seconds)
  if (now - lastDebugTime > 10000) {
    lastDebugTime = now;
    Serial.printf("[DEBUG] Still running... WiFi: %s\n",
                  isConnected ? "Connected" : "Disconnected");
  }

  // Try to read a card
  String uid;
  bool cardRead = false;
  if (!isCardFlowState(currentVisualState)) {
    cardRead = rfid.readCard(uid);
  }
  
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
      setVisualState(VisualState::CardDetected, now);

      if (!wifi.isConnected()) {
        Serial.println("[ERROR] Not connected to Wi-Fi. Skipping backend request.");
        setVisualState(VisualState::BackendError, millis());
        now = millis();
      } else if (pendingBackendRequest || backend.isBusy()) {
        Serial.println("[Backend] Request already in progress. Ignoring new card.");
      } else {
        Serial.println("Starting asynchronous request to backend...");
        if (backend.beginPostPlayAsync(uid)) {
          pendingBackendRequest = true;
          now = millis();
          setVisualState(VisualState::CardScanning, now);
        } else {
          Serial.println("[ERROR] Failed to start backend request");
          now = millis();
          setVisualState(VisualState::BackendError, now);
        }
      }
      Serial.println("*** END CARD PROCESSING ***\n");
    }
  }

  if (pendingBackendRequest) {
    bool success = false;
    if (backend.pollResult(success)) {
      pendingBackendRequest = false;
      now = millis();
      if (success) {
        Serial.println("[SUCCESS] Backend request successful");
        setVisualState(VisualState::BackendSuccess, now);
      } else {
        Serial.println("[ERROR] Backend request failed");
        setVisualState(VisualState::BackendError, now);
      }
    }
  }

  now = millis();
  refreshVisualState(now);
  effects.update(now);
}
