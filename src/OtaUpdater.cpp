/*
 * OtaUpdater.cpp
 *
 * Implements periodic OTA update checks against a remote manifest.
 */

#include "OtaUpdater.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <HttpClient.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClient.h>

#include "Config.h"

namespace {
constexpr unsigned long kDefaultManifestTimeoutMs = 10000;
}

OtaUpdater::OtaUpdater() : _lastCheckAt(0), _checkedSinceBoot(false) {}

void OtaUpdater::loop(unsigned long now, bool wifiConnected) {
  if (!wifiConnected) {
    return;
  }
  if (!shouldCheck(now)) {
    return;
  }
  checkForUpdates();
  scheduleAfterAttempt(now);
}

bool OtaUpdater::shouldCheck(unsigned long now) const {
  if (!_checkedSinceBoot) {
    return true;
  }
  unsigned long elapsed = now - _lastCheckAt;
  return elapsed >= OTA_CHECK_INTERVAL_MS;
}

void OtaUpdater::scheduleAfterAttempt(unsigned long now) {
  _lastCheckAt = now;
  _checkedSinceBoot = true;
}

void OtaUpdater::checkForUpdates() {
  Serial.println("[OTA] Checking for firmware updates...");

  String remoteVersion;
  String firmwareUrl;
  if (!fetchManifest(remoteVersion, firmwareUrl)) {
    Serial.println("[OTA] Manifest fetch failed.");
    return;
  }

  Serial.printf("[OTA] Current version: %s\n", CURRENT_FIRMWARE_VERSION);
  Serial.printf("[OTA] Remote version: %s\n", remoteVersion.c_str());

  int comparison = compareVersions(remoteVersion, CURRENT_FIRMWARE_VERSION);
  if (comparison <= 0) {
    Serial.println("[OTA] Device firmware is up to date.");
    return;
  }

  Serial.println("[OTA] Newer firmware detected. Starting download...");
  if (!downloadAndInstall(firmwareUrl, remoteVersion)) {
    Serial.println("[OTA] Firmware download or install failed.");
  }
}

bool OtaUpdater::fetchManifest(String &versionOut, String &firmwareUrlOut) {
  String host;
  if (!resolveHost(host)) {
    return false;
  }

  WiFiClient netClient;
  HttpClient httpClient(netClient, host.c_str(), BACKEND_PORT);
  httpClient.setTimeout(OTA_HTTP_TIMEOUT_MS > 0 ? OTA_HTTP_TIMEOUT_MS :
                                                kDefaultManifestTimeoutMs);

  int statusCode = httpClient.get(OTA_MANIFEST_PATH);
  if (statusCode < 0) {
    Serial.printf("[OTA] HTTP connection failed: %d\n", statusCode);
    httpClient.stop();
    return false;
  }

  int responseCode = httpClient.responseStatusCode();
  String responseBody = httpClient.responseBody();
  httpClient.stop();

  if (responseCode != 200) {
    Serial.printf("[OTA] Manifest request returned HTTP %d\n", responseCode);
    return false;
  }

  if (responseBody.length() == 0) {
    Serial.println("[OTA] Manifest body was empty.");
    return false;
  }

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, responseBody);
  if (err) {
    Serial.printf("[OTA] Failed to parse manifest JSON: %s\n",
                  err.c_str());
    return false;
  }

  const char *version = doc["version"] | nullptr;
  const char *firmwareUrl = doc["firmware_url"] | nullptr;
  if (version == nullptr || firmwareUrl == nullptr) {
    Serial.println("[OTA] Manifest missing required fields.");
    return false;
  }

  versionOut = version;
  firmwareUrlOut = firmwareUrl;
  return true;
}

bool OtaUpdater::resolveHost(String &hostOut) {
  hostOut = String(BACKEND_HOST);
  if (!hostOut.endsWith(".local")) {
    return true;
  }

  String hostname = hostOut;
  hostname.replace(".local", "");
  Serial.printf("[OTA] Resolving mDNS host %s.local...\n", hostname.c_str());
  IPAddress resolved = MDNS.queryHost(hostname);
  if (resolved == IPAddress(0, 0, 0, 0)) {
    Serial.println("[OTA] Failed to resolve manifest host via mDNS.");
    return false;
  }

  hostOut = resolved.toString();
  Serial.printf("[OTA] Manifest host resolved to %s\n", hostOut.c_str());
  return true;
}

bool OtaUpdater::downloadAndInstall(const String &url, const String &newVersion) {
  HTTPClient client;
  WiFiClient downloadClient;
  if (!client.begin(downloadClient, url)) {
    Serial.println("[OTA] Unable to begin firmware download.");
    return false;
  }

  int httpCode = client.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[OTA] Firmware download failed with HTTP %d\n", httpCode);
    client.end();
    return false;
  }

  int contentLength = client.getSize();
  if (!Update.begin(contentLength > 0 ? contentLength : UPDATE_SIZE_UNKNOWN)) {
    Serial.println("[OTA] Update.begin() failed.");
    client.end();
    return false;
  }

  WiFiClient *stream = client.getStreamPtr();
  size_t written = Update.writeStream(*stream);
  if (written == 0) {
    Serial.println("[OTA] No data written during update.");
    client.end();
    return false;
  }

  if (!Update.end()) {
    Serial.printf("[OTA] Update failed: %s\n", Update.errorString());
    client.end();
    return false;
  }

  if (!Update.isFinished()) {
    Serial.println("[OTA] Update did not complete successfully.");
    client.end();
    return false;
  }

  Serial.printf("[OTA] Firmware %s installed successfully. Rebooting...\n",
                newVersion.c_str());
  client.end();
  delay(100);
  ESP.restart();
  return true;
}

int OtaUpdater::compareVersions(const String &lhs, const String &rhs) {
  size_t i = 0;
  size_t j = 0;

  auto readSegment = [](const String &value, size_t &index) {
    long segment = 0;
    bool hasDigits = false;
    while (index < value.length() && value[index] != '.') {
      char c = value[index];
      if (c >= '0' && c <= '9') {
        segment = segment * 10 + (c - '0');
        hasDigits = true;
      }
      index++;
    }
    if (index < value.length() && value[index] == '.') {
      index++;
    }
    return hasDigits ? segment : 0;
  };

  while (i < lhs.length() || j < rhs.length()) {
    long lhsSegment = readSegment(lhs, i);
    long rhsSegment = readSegment(rhs, j);
    if (lhsSegment < rhsSegment) {
      return -1;
    }
    if (lhsSegment > rhsSegment) {
      return 1;
    }
  }

  return 0;
}

