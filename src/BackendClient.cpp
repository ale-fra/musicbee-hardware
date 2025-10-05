/*
 * BackendClient.cpp
 *
 * Implements communication with the jukebox backend service using
 * ArduinoHttpClient with mDNS support for .local domains.
 */

#include "BackendClient.h"
#include "Config.h"
#include <WiFiClient.h>
#include <HttpClient.h>
#include <ESPmDNS.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

bool BackendClient::postPlay(const String &cardUid) {
  return performPostPlay(cardUid);
}

bool BackendClient::beginPostPlayAsync(const String &cardUid) {
  if (requestInProgress) {
    Serial.println("[Backend] Ignoring async request while another is running");
    return false;
  }

  if (cardUid.length() == 0) {
    Serial.println("[Backend] Empty UID provided to beginPostPlayAsync");
    return false;
  }

  pendingUid = cardUid;
  requestInProgress = true;
  requestCompleted = false;
  lastRequestSuccess = false;

  BaseType_t created = xTaskCreate(BackendClient::requestTask, "BackendPostPlay", 4096, this, 1, nullptr);
  if (created != pdPASS) {
    Serial.println("[Backend] Failed to start backend task");
    requestInProgress = false;
    pendingUid = "";
    return false;
  }

  return true;
}

bool BackendClient::isBusy() const {
  return requestInProgress;
}

bool BackendClient::pollResult(bool &outSuccess) {
  if (!requestCompleted) {
    return false;
  }

  outSuccess = lastRequestSuccess;
  requestCompleted = false;
  return true;
}

bool BackendClient::performPostPlay(const String &cardUid) {
  // Guard: ensure we have a valid UID
  if (cardUid.length() == 0) {
    Serial.println("[Backend] Empty UID provided to postPlay");
    return false;
  }

  // Build the request path
  String path = String("/api/v1/cards/") + cardUid + "/play";
  
  String targetHost = String(BACKEND_HOST);
  IPAddress serverIP;
  
  Serial.printf("[Backend] Target: %s:%d\n", BACKEND_HOST, BACKEND_PORT);
  Serial.printf("[Backend] Path: %s\n", path.c_str());
  
  // Check if host is a .local domain (mDNS)
  if (targetHost.endsWith(".local")) {
    Serial.println("[Backend] Resolving mDNS hostname...");
    
    // Remove .local suffix for mDNS query
    String hostname = targetHost;
    hostname.replace(".local", "");
    
    // Try to resolve mDNS
    serverIP = MDNS.queryHost(hostname);
    
    if (serverIP == IPAddress(0, 0, 0, 0)) {
      Serial.println("[Backend] ERROR: mDNS resolution failed");
      Serial.println("[Backend] Make sure:");
      Serial.println("  - Backend server is running");
      Serial.println("  - mDNS service is active on backend");
      Serial.println("  - Both devices are on same network");
      return false;
    }
    
    Serial.printf("[Backend] mDNS resolved to: %s\n", serverIP.toString().c_str());
    targetHost = serverIP.toString();
  }

  Serial.println("[Backend] Starting HTTP request...");

  // Set up the HTTP client
  WiFiClient netClient;
  HttpClient httpClient(netClient, targetHost.c_str(), BACKEND_PORT);
  
  // Set a reasonable timeout
  httpClient.setTimeout(5000);

  // Begin the request
  int statusCode = httpClient.post(path, "application/json", "{}");
  if (statusCode < 0) {
    Serial.printf("[Backend] ERROR: Connection failed with code: %d\n", statusCode);
    Serial.println("[Backend] Possible causes:");
    Serial.println("  - Backend server not running");
    Serial.println("  - Wrong host/port in secrets.h");
    Serial.println("  - Network connectivity issues");
    Serial.println("  - Firewall blocking connection");
    httpClient.stop();
    return false;
  }

  // Read the response status
  int responseCode = httpClient.responseStatusCode();
  String responseBody = httpClient.responseBody();
  httpClient.stop();

  Serial.printf("[Backend] Response code: %d\n", responseCode);
  if (responseBody.length() > 0) {
    Serial.printf("[Backend] Response body: %s\n", responseBody.c_str());
  }

  // Success if 2xx
  return (responseCode >= 200 && responseCode < 300);
}

void BackendClient::requestTask(void *param) {
  auto *client = static_cast<BackendClient *>(param);
  String uid = client->pendingUid;
  bool success = client->performPostPlay(uid);

  client->pendingUid = "";
  client->lastRequestSuccess = success;
  client->requestCompleted = true;
  client->requestInProgress = false;

  vTaskDelete(nullptr);
}
