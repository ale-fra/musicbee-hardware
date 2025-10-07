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
#include "ActionCards.h"
#include "WifiManager.h"
#include "RfidReader.h"
#include "BackendClient.h"
#include "EffectManager.h"
#include "OtaUpdater.h"

#if ENABLE_DEBUG_ACTIONS
#  include "DebugActionServer.h"
#endif

static WifiManager wifi;
static RfidReader rfid;
static BackendClient backend;
static EffectManager effects(LED_DATA_PIN, LED_COUNT_DEFAULT, LED_BRIGHTNESS_DEFAULT);
static OtaUpdater otaUpdater;
static ActionCardRegistry actionCards(ACTION_CARD_MAPPINGS, ACTION_CARD_MAPPING_COUNT);

namespace {
constexpr float kTailPrimaryFactor = 0.5f;
constexpr float kTailSecondaryFactor = 0.2f;
constexpr unsigned long kWifiCometIntervalMs = 40;
constexpr unsigned long kSuccessSpinIntervalMs = 28;
constexpr unsigned long kErrorFadeDurationMs = 300;
constexpr unsigned long kTransientEffectDurationMs = 2500;
constexpr unsigned long kCardRainbowIntervalMs = 15;
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

class VisualStateController {
public:
  explicit VisualStateController(EffectManager &effects) : _effects(effects) {}

  VisualState currentState() const { return _currentState; }

  void setState(VisualState state, unsigned long now) {
    if (_initialized && _currentState == state) {
      return;
    }
    if (_currentState != state) {
      Serial.printf("[State] Transitioning from %s to %s at %lums\n",
                    stateName(_currentState), stateName(state), now);
    }
    _currentState = state;
    _stateChangedAt = now;
    _initialized = true;
    applyState(state, now);
  }

  void setBaseState(VisualState state, unsigned long now) {
    _baseState = state;
    bool shouldApply = !isTransientState(_currentState) || _currentState == state;
    if (_currentState == VisualState::CardScanning && _backendPending) {
      shouldApply = false;
    }
    if (shouldApply) {
      setState(state, now);
    }
  }

  void refresh(unsigned long now) {
    if (isTransientState(_currentState) &&
        now - _stateChangedAt >= kTransientEffectDurationMs) {
      if (_backendPending) {
        setState(VisualState::CardScanning, now);
      } else {
        setState(_baseState, now);
      }
    }
  }

  void updateWifiState(bool isConnected, bool wasPreviouslyConnected, unsigned long now) {
    VisualState target = VisualState::WifiConnecting;
    if (isConnected) {
      target = VisualState::Idle;
    } else if (_initialized && wasPreviouslyConnected) {
      target = VisualState::WifiError;
    }
    setBaseState(target, now);
  }

  void onBackendRequestStarted(unsigned long now) {
    _backendPending = true;
    setState(VisualState::CardScanning, now);
  }

  void onBackendRequestFinished(bool success, unsigned long now) {
    _backendPending = false;
    setState(success ? VisualState::BackendSuccess : VisualState::BackendError, now);
  }

  bool isBackendPending() const { return _backendPending; }

  static bool isCardFlowState(VisualState state) {
    return state == VisualState::CardDetected || state == VisualState::BackendSuccess ||
           state == VisualState::BackendError;
  }

  static bool isTransientState(VisualState state) {
    return isCardFlowState(state) || state == VisualState::MdnsSuccess ||
           state == VisualState::MdnsError;
  }

private:
  static const char *stateName(VisualState state) {
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

  void applyState(VisualState state, unsigned long now) {
    switch (state) {
      case VisualState::Idle:
        _effects.turnOff(now);
        break;
      case VisualState::WifiConnecting:
        _effects.showComet(0, 0, 255,
                           kTailPrimaryFactor, kTailSecondaryFactor,
                           CometEffect::Direction::Clockwise,
                           kWifiCometIntervalMs, now);
        break;
      case VisualState::WifiConnected:
        _effects.turnOff(now);
        break;
      case VisualState::WifiError:
        _effects.showFade(255, 0, 0, 80, 0, 0, kErrorFadeDurationMs, now);
        break;
      case VisualState::CardDetected:
        _effects.showRainbow(kCardRainbowIntervalMs, now);
        break;
      case VisualState::CardScanning:
        _effects.showRainbow(kCardRainbowIntervalMs, now);
        break;
      case VisualState::BackendSuccess:
        _effects.snakeEffect().setInterval(kSuccessSpinIntervalMs);
        _effects.showSnake(0, 255, 0, now);
        break;
      case VisualState::BackendError:
        _effects.showFade(255, 0, 0, 0, 0, 0, kErrorFadeDurationMs, now);
        break;
      case VisualState::MdnsResolving:
        _effects.showComet(0, 128, 255,
                           kTailPrimaryFactor, kTailSecondaryFactor,
                           CometEffect::Direction::Clockwise,
                           kWifiCometIntervalMs, now);
        break;
      case VisualState::MdnsSuccess:
        _effects.showSolidColor(0, 64, 0, now);
        break;
      case VisualState::MdnsError:
        _effects.showFade(255, 32, 32, 0, 0, 0, kErrorFadeDurationMs, now);
        break;
    }
  }

  EffectManager &_effects;
  VisualState _baseState = VisualState::WifiConnecting;
  VisualState _currentState = VisualState::WifiConnecting;
  unsigned long _stateChangedAt = 0;
  bool _backendPending = false;
  bool _initialized = false;
};

static VisualStateController visualState(effects);
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
  if (!VisualStateController::isCardFlowState(visualState.currentState())) {
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

static void setVisualState(VisualState state, unsigned long now) {
  visualState.setState(state, now);
}

static void setBaseVisualState(VisualState state, unsigned long now) {
  visualState.setBaseState(state, now);
}

static void refreshVisualState(unsigned long now) {
  visualState.refresh(now);
}

static void updateWifiVisualState(bool isConnected, unsigned long now) {
  visualState.updateWifiState(isConnected, wifiPreviouslyConnected, now);
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
  BackendPending,
  BackendFailure,
  WifiDisconnected,
  BackendBusy,
  ActionHandled
};

static void handleBackendCompletion(bool success, unsigned long now);
static CardProcessResult startBackendRequest(const String &uid);
static bool handleActionCard(const ActionCardEntry &entry, unsigned long now);
static const char *actionCardName(ActionCardType type);

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

  const ActionCardEntry *actionEntry = actionCards.findByUid(uid);
  if (actionEntry != nullptr) {
    Serial.printf("[ActionCard] Matched %s command card.\n",
                  actionCardName(actionEntry->type));
    if (handleActionCard(*actionEntry, now)) {
      Serial.println("*** END CARD PROCESSING ***\n");
      return CardProcessResult::ActionHandled;
    }
    Serial.println("[ActionCard] No handler executed for this command card.");
  }

  CardProcessResult backendResult = startBackendRequest(uid);
  if (backendResult != CardProcessResult::BackendPending) {
    Serial.println("*** END CARD PROCESSING ***\n");
  }
  return backendResult;
}

static CardProcessResult startBackendRequest(const String &uid) {
  if (!wifi.isConnected()) {
    Serial.println("[ERROR] Not connected to Wi-Fi. Skipping backend request.");
    unsigned long now = millis();
    setVisualState(VisualState::BackendError, now);
    return CardProcessResult::WifiDisconnected;
  }

  if (visualState.isBackendPending() || backend.isBusy()) {
    Serial.println("[Backend] Request already in progress. Ignoring new card.");
    return CardProcessResult::BackendBusy;
  }

  Serial.println("Starting asynchronous request to backend...");
  if (backend.beginPostPlayAsync(uid)) {
    unsigned long now = millis();
    visualState.onBackendRequestStarted(now);
    return CardProcessResult::BackendPending;
  }

  Serial.println("[ERROR] Failed to start backend request");
  unsigned long now = millis();
  setVisualState(VisualState::BackendError, now);
  return CardProcessResult::BackendFailure;
}

static const char *actionCardName(ActionCardType type) {
  switch (type) {
    case ActionCardType::Reset:
      return "reset";
  }
  return "unknown";
}

static bool handleActionCard(const ActionCardEntry &entry, unsigned long now) {
  switch (entry.type) {
    case ActionCardType::Reset: {
      Serial.println("[ActionCard] Reset command received. Rebooting device...");
      setVisualState(VisualState::BackendSuccess, now);
      effects.update(now);
      Serial.flush();
      delay(250);
      ESP.restart();
      return true;
    }
  }

  Serial.println("[ActionCard] Handler not implemented for this action type.");
  return false;
}

static void handleBackendCompletion(bool success, unsigned long now) {
  if (success) {
    Serial.println("[SUCCESS] Backend request successful");
  } else {
    Serial.println("[ERROR] Backend request failed");
  }
  visualState.onBackendRequestFinished(success, now);
  Serial.println("*** END CARD PROCESSING ***\n");
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
  if (normalized == "idle") {
    state = VisualState::Idle;
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

  if (result == CardProcessResult::BackendPending) {
    bool success = false;
    Serial.println("[Debug] Waiting for backend request triggered via debug action...");
    while (!backend.pollResult(success)) {
      delay(10);
      yield();
    }
    unsigned long completionNow = millis();
    handleBackendCompletion(success, completionNow);
    if (success) {
      message = "Backend request completed successfully.";
      return true;
    }
    message = "Backend request failed.";
    return false;
  }

  switch (result) {
    case CardProcessResult::DuplicateIgnored:
      message = "Duplicate UID ignored due to debounce.";
      return false;
    case CardProcessResult::BackendSkipped:
      message = "Simulated card processed without backend call.";
      return true;
    case CardProcessResult::BackendFailure:
      message = "Failed to start backend request.";
      return false;
    case CardProcessResult::WifiDisconnected:
      message = "Wi-Fi is disconnected; backend request skipped.";
      return false;
    case CardProcessResult::BackendBusy:
      message = "Backend request already in progress.";
      return false;
    case CardProcessResult::ActionHandled:
      message = "Command card executed successfully.";
      return true;
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
  Serial.printf("[ActionCard] %u command card(s) configured.\n",
                static_cast<unsigned>(ACTION_CARD_MAPPING_COUNT));
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

  otaUpdater.loop(now, isConnected);

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
  if (!VisualStateController::isCardFlowState(visualState.currentState())) {
    cardRead = rfid.readCard(uid);
  }

  if (cardRead) {
    now = millis();
    processCardUid(uid, now, false, true);
  }

  bool success = false;
  if (backend.pollResult(success)) {
    now = millis();
    handleBackendCompletion(success, now);
  }

  now = millis();
  refreshVisualState(now);
  effects.update(now);
}
