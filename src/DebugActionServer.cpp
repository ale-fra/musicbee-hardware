/*
 * DebugActionServer.cpp
 *
 * Implements a small HTTP server that exposes debug-friendly endpoints
 * for interacting with firmware behaviour at runtime. This allows
 * developers to trigger actions over the network instead of relying on
 * physical inputs.
 */

#include "DebugActionServer.h"

#include <Arduino.h>
#include <uri/UriBraces.h>

#include "Config.h"

namespace {
constexpr size_t kListPayloadCapacity = 768;
constexpr size_t kActionPayloadCapacity = 512;
}

DebugActionServer::DebugActionServer(uint16_t port) : server_(port) {}

void DebugActionServer::registerAction(const DebugAction &action) {
  actions_.push_back(action);
}

void DebugActionServer::begin() {
  if (routesRegistered_) {
    return;
  }

  setupRoutes();
  routesRegistered_ = true;
}

void DebugActionServer::start() {
  if (!routesRegistered_) {
    begin();
  }

  if (running_) {
    return;
  }

  server_.begin();
  running_ = true;
  Serial.printf("[DebugServer] Started on port %u\n", server_.serverPort());
}

void DebugActionServer::stop() {
  if (!running_) {
    return;
  }

  server_.stop();
  running_ = false;
  Serial.println("[DebugServer] Stopped");
}

void DebugActionServer::loop() {
  if (!running_) {
    return;
  }

  server_.handleClient();
}

void DebugActionServer::setupRoutes() {
  server_.on("/debug/actions", HTTP_GET, [this]() { handleListActions(); });

  server_.on(UriBraces("/debug/actions/{}"), HTTP_POST,
             [this]() { handleInvokeAction(); });

  server_.onNotFound([this]() {
    StaticJsonDocument<128> doc;
    doc["ok"] = false;
    doc["message"] = "Endpoint not found";
    sendJson(404, doc);
  });
}

void DebugActionServer::handleListActions() {
  StaticJsonDocument<kListPayloadCapacity> doc;
  JsonArray actions = doc.createNestedArray("actions");

  for (const DebugAction &action : actions_) {
    JsonObject entry = actions.createNestedObject();
    entry["name"] = action.name;
    entry["description"] = action.description;
  }

  doc["ok"] = true;
  sendJson(200, doc);
}

void DebugActionServer::handleInvokeAction() {
  if (server_.pathArg(0).isEmpty()) {
    StaticJsonDocument<128> doc;
    doc["ok"] = false;
    doc["message"] = "Missing action name";
    sendJson(400, doc);
    return;
  }

  const String actionName = server_.pathArg(0);
  const DebugAction *target = nullptr;
  for (const DebugAction &candidate : actions_) {
    if (actionName.equalsIgnoreCase(candidate.name)) {
      target = &candidate;
      break;
    }
  }

  if (target == nullptr) {
    StaticJsonDocument<128> doc;
    doc["ok"] = false;
    doc["message"] = "Unknown action";
    sendJson(404, doc);
    return;
  }

  StaticJsonDocument<kActionPayloadCapacity> payloadDoc;
  DeserializationError err = deserializeJson(payloadDoc, server_.arg("plain"));
  if (err) {
    StaticJsonDocument<160> doc;
    doc["ok"] = false;
    doc["message"] = String("Invalid JSON payload: ") + err.c_str();
    sendJson(400, doc);
    return;
  }

  JsonVariantConst payload = payloadDoc.as<JsonVariantConst>();
  String message;
  bool ok = target->handler(payload, message);

  StaticJsonDocument<256> response;
  response["ok"] = ok;
  response["message"] = message;

  int status = ok ? 200 : 400;
  sendJson(status, response);
}

void DebugActionServer::sendJson(int statusCode, const JsonDocument &doc) {
  String body;
  serializeJson(doc, body);
  server_.send(statusCode, "application/json", body);
}

