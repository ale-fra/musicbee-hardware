/*
 * OtaUpdater.cpp
 *
 * Implements periodic OTA update checks against a remote manifest.
 */

#include "OtaUpdater.h"

#include <ArduinoJson.h>
#include <HttpClient.h>
#include <Update.h>
#include <WiFiClient.h>

#include "BackendClient.h"
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
  String manifestHost;
  if (!fetchManifest(remoteVersion, firmwareUrl, manifestHost)) {
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
  if (!downloadAndInstall(firmwareUrl, manifestHost, remoteVersion)) {
    Serial.println("[OTA] Firmware download or install failed.");
  }
}

bool OtaUpdater::fetchManifest(String &versionOut, String &firmwareUrlOut,
                               String &resolvedHostOut) {
  Serial.println("[OTA] ========== MANIFEST FETCH START ==========");
  
  // Resolve hostname
  Serial.printf("[OTA] Resolving hostname: %s\n", BACKEND_HOST);
  if (!BackendClient::resolveHostname(String(BACKEND_HOST), resolvedHostOut)) {
    Serial.println("[OTA] ERROR: Hostname resolution failed");
    return false;
  }
  Serial.printf("[OTA] Resolved to: %s\n", resolvedHostOut.c_str());

  // Setup HTTP client
  WiFiClient netClient;
  unsigned long timeout = OTA_HTTP_TIMEOUT_MS > 0 ? OTA_HTTP_TIMEOUT_MS : kDefaultManifestTimeoutMs;
  Serial.printf("[OTA] Setting network timeout: %lu ms\n", timeout);
  netClient.setTimeout(timeout);
  
  HttpClient httpClient(netClient, resolvedHostOut.c_str(), BACKEND_PORT);
  httpClient.setTimeout(timeout);
  Serial.printf("[OTA] HttpClient configured for %s:%d\n", resolvedHostOut.c_str(), BACKEND_PORT);

  // Build manifest path
  String manifestPath = String(BACKEND_API_PREFIX) + OTA_MANIFEST_PATH;
  Serial.printf("[OTA] Requesting: %s\n", manifestPath.c_str());
  Serial.printf("[OTA] Full URL: http://%s:%d%s\n", resolvedHostOut.c_str(), BACKEND_PORT, manifestPath.c_str());

  // Make the request
  Serial.println("[OTA] Sending GET request...");
  int statusCode = httpClient.get(manifestPath);
  if (statusCode < 0) {
    Serial.printf("[OTA] ERROR: HTTP connection failed with code: %d\n", statusCode);
    httpClient.stop();
    return false;
  }
  Serial.printf("[OTA] Connection status code: %d\n", statusCode);

  // Get response code
  int responseCode = httpClient.responseStatusCode();
  Serial.printf("[OTA] HTTP response code: %d\n", responseCode);
  
  if (responseCode != 200) {
    Serial.printf("[OTA] ERROR: Unexpected response code: %d\n", responseCode);
    httpClient.stop();
    return false;
  }

  // Read response body
  Serial.println("[OTA] Reading response body...");
  String responseBody = httpClient.responseBody();
  httpClient.stop();
  
  Serial.printf("[OTA] Response body length: %d bytes\n", responseBody.length());
  
  if (responseBody.length() == 0) {
    Serial.println("[OTA] ERROR: Response body is empty");
    return false;
  }
  
  // Print the raw response (truncate if too long)
  if (responseBody.length() <= 200) {
    Serial.printf("[OTA] Response body: '%s'\n", responseBody.c_str());
  } else {
    Serial.printf("[OTA] Response body (first 200 chars): '%s...'\n", responseBody.substring(0, 200).c_str());
  }
  
  // Print hex dump of first 50 bytes to check for hidden characters
  Serial.print("[OTA] Response hex (first 50 bytes): ");
  for (size_t i = 0; i < min((size_t)50, (size_t)responseBody.length()); i++) {
    Serial.printf("%02X ", (unsigned char)responseBody[i]);
  }
  Serial.println();

  // Parse JSON
  Serial.println("[OTA] Parsing JSON...");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, responseBody);
  
  if (err) {
    Serial.printf("[OTA] ERROR: JSON parsing failed: %s\n", err.c_str());
    Serial.printf("[OTA] Error code: %d\n", (int)err.code());
    return false;
  }
  
  Serial.println("[OTA] JSON parsed successfully!");

  // Check for required fields (ArduinoJson v7 style)
  bool hasVersion = !doc["version"].isNull();
  bool hasFirmwareUrl = !doc["firmware_url"].isNull();
  
  Serial.printf("[OTA] Field 'version' present: %s\n", hasVersion ? "YES" : "NO");
  Serial.printf("[OTA] Field 'firmware_url' present: %s\n", hasFirmwareUrl ? "YES" : "NO");

  // Try to extract fields (ArduinoJson v7 style)
  const char *version = doc["version"].as<const char*>();
  const char *firmwareUrl = doc["firmware_url"].as<const char*>();
  
  Serial.printf("[OTA] version pointer: %s\n", version ? "NOT NULL" : "NULL");
  Serial.printf("[OTA] firmwareUrl pointer: %s\n", firmwareUrl ? "NOT NULL" : "NULL");
  
  if (version != nullptr) {
    Serial.printf("[OTA] version value: '%s'\n", version);
  }
  
  if (firmwareUrl != nullptr) {
    Serial.printf("[OTA] firmwareUrl value: '%s'\n", firmwareUrl);
  }
  
  // Final check
  if (version == nullptr || firmwareUrl == nullptr) {
    Serial.println("[OTA] ERROR: Missing required fields");
    if (version == nullptr) Serial.println("[OTA]   - 'version' is null");
    if (firmwareUrl == nullptr) Serial.println("[OTA]   - 'firmware_url' is null");
    return false;
  }

  versionOut = version;
  firmwareUrlOut = firmwareUrl;
  
  Serial.println("[OTA] Manifest fetch successful!");
  Serial.println("[OTA] ========== MANIFEST FETCH END ==========");
  return true;
}

bool OtaUpdater::downloadAndInstall(const String &url,
                                    const String &manifestHost,
                                    const String &newVersion) {
  struct FirmwareRequest {
    String connectionHost;
    String hostHeader;
    uint16_t port;
    String path;
  } request;

  auto makeHostHeader = [](const String &host, uint16_t port) {
    if (port == 80) {
      return host;
    }
    String header(host);
    header += ':';
    header += port;
    return header;
  };

  request.port = BACKEND_PORT;
  request.connectionHost = manifestHost;
  request.hostHeader = makeHostHeader(String(BACKEND_HOST), request.port);

  String trimmedUrl = url;
  trimmedUrl.trim();
  if (trimmedUrl.isEmpty()) {
    Serial.println("[OTA] Firmware URL from manifest was empty.");
    return false;
  }

  bool isHttpUrl = trimmedUrl.startsWith("http://") ||
                   trimmedUrl.startsWith("https://");
  if (isHttpUrl) {
    if (trimmedUrl.startsWith("https://")) {
      Serial.println("[OTA] HTTPS firmware URLs are not supported by the OTA updater.");
      return false;
    }

    int schemeLength = 7;  // length of "http://"
    int pathIndex = trimmedUrl.indexOf('/', schemeLength);
    String hostPort =
        pathIndex < 0 ? trimmedUrl.substring(schemeLength)
                      : trimmedUrl.substring(schemeLength, pathIndex);
    if (hostPort.isEmpty()) {
      Serial.println("[OTA] Firmware URL missing host component.");
      return false;
    }

    String path = pathIndex < 0 ? String("/") : trimmedUrl.substring(pathIndex);
    int colonIndex = hostPort.lastIndexOf(':');
    String hostOnly = colonIndex >= 0 ? hostPort.substring(0, colonIndex)
                                      : hostPort;
    uint16_t port = colonIndex >= 0 ? hostPort.substring(colonIndex + 1).toInt()
                                    : 80;
    if (port == 0) {
      port = 80;
    }

    String resolved;
    if (!BackendClient::resolveHostname(hostOnly, resolved)) {
      return false;
    }

    request.connectionHost = resolved;
    request.hostHeader = makeHostHeader(hostOnly, port);
    request.port = port;
    request.path = path;
  } else {
    if (trimmedUrl.startsWith("/")) {
      request.path = trimmedUrl;
    } else {
      String prefix = String(BACKEND_API_PREFIX);
      if (!prefix.endsWith("/")) {
        prefix += '/';
      }
      request.path = prefix + trimmedUrl;
    }
  }

  WiFiClient downloadClient;
  downloadClient.setTimeout(OTA_HTTP_TIMEOUT_MS > 0 ? OTA_HTTP_TIMEOUT_MS :
                                                       kDefaultManifestTimeoutMs);

  Serial.printf("[OTA] Connecting to %s:%u for firmware download...\n",
                request.connectionHost.c_str(), request.port);
  if (!downloadClient.connect(request.connectionHost.c_str(), request.port)) {
    Serial.println("[OTA] Connection to firmware host failed.");
    return false;
  }

  downloadClient.printf("GET %s HTTP/1.1\r\n", request.path.c_str());
  downloadClient.printf("Host: %s\r\n", request.hostHeader.c_str());
  downloadClient.print("Connection: close\r\n");
  downloadClient.print("User-Agent: MusicBee-OTA/1.0\r\n\r\n");

  unsigned long waitStart = millis();
  while (downloadClient.connected() && !downloadClient.available()) {
    if (OTA_HTTP_TIMEOUT_MS > 0 &&
        millis() - waitStart > OTA_HTTP_TIMEOUT_MS) {
      Serial.println("[OTA] Firmware host timed out before sending data.");
      downloadClient.stop();
      return false;
    }
    delay(10);
  }

  String statusLine = downloadClient.readStringUntil('\n');
  statusLine.trim();
  if (!statusLine.startsWith("HTTP/")) {
    Serial.println("[OTA] Invalid HTTP response from firmware host.");
    downloadClient.stop();
    return false;
  }

  int firstSpace = statusLine.indexOf(' ');
  int secondSpace = statusLine.indexOf(' ', firstSpace + 1);
  String codeStr =
      firstSpace >= 0 ? statusLine.substring(firstSpace + 1, secondSpace > 0
                                                                ? secondSpace
                                                                : statusLine.length())
                      : String();
  int statusCode = codeStr.toInt();
  if (statusCode != 200) {
    Serial.printf("[OTA] Firmware download failed with HTTP %d\n", statusCode);
    downloadClient.stop();
    return false;
  }

  long contentLength = -1;
  while (downloadClient.connected()) {
    String line = downloadClient.readStringUntil('\n');
    if (line.length() == 0) {
      break;
    }
    line.trim();
    if (line.length() == 0) {
      break;
    }

    int colonIndex = line.indexOf(':');
    if (colonIndex < 0) {
      continue;
    }

    String headerName = line.substring(0, colonIndex);
    String headerValue = line.substring(colonIndex + 1);
    headerValue.trim();
    headerName.toLowerCase();
    if (headerName == "content-length") {
      contentLength = headerValue.toInt();
    }
  }

  if (!Update.begin(contentLength > 0 ? contentLength : UPDATE_SIZE_UNKNOWN)) {
    Serial.println("[OTA] Update.begin() failed.");
    downloadClient.stop();
    return false;
  }

  size_t written = Update.writeStream(downloadClient);
  if (written == 0) {
    Serial.println("[OTA] No data written during update.");
    downloadClient.stop();
    return false;
  }

  if (!Update.end()) {
    Serial.printf("[OTA] Update failed: %s\n", Update.errorString());
    downloadClient.stop();
    return false;
  }

  if (!Update.isFinished()) {
    Serial.println("[OTA] Update did not complete successfully.");
    downloadClient.stop();
    return false;
  }

  Serial.printf("[OTA] Firmware %s installed successfully. Rebooting...\n",
                newVersion.c_str());
  downloadClient.stop();
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