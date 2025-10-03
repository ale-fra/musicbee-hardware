/*
 * DebugActionServer.h
 *
 * Lightweight HTTP server that exposes firmware debug actions via the
 * ESP32 WebServer. Actions are registered at runtime and can be invoked
 * with JSON payloads to simulate card reads or preview LED effects
 * without touching the physical hardware.
 */

#pragma once

#include <ArduinoJson.h>
#include <WebServer.h>
#include <vector>

struct DebugAction {
  const char *name;
  const char *description;
  bool (*handler)(JsonVariantConst payload, String &message);
};

class DebugActionServer {
 public:
  explicit DebugActionServer(uint16_t port);

  void registerAction(const DebugAction &action);
  void begin();
  void start();
  void stop();
  void loop();

 private:
  void setupRoutes();
  void handleListActions();
  void handleInvokeAction();
  void sendJson(int statusCode, const JsonDocument &doc);

  WebServer server_;
  std::vector<DebugAction> actions_;
  bool routesRegistered_ = false;
  bool running_ = false;
};

