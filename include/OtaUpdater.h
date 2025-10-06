/*
 * OtaUpdater.h
 *
 * Declares a helper responsible for periodically checking a remote
 * manifest for new firmware versions and applying OTA updates when
 * available.
 */

#pragma once

#include <Arduino.h>

class OtaUpdater {
public:
  OtaUpdater();

  void loop(unsigned long now, bool wifiConnected);

private:
  bool shouldCheck(unsigned long now) const;
  void scheduleAfterAttempt(unsigned long now);
  void checkForUpdates();
  bool fetchManifest(String &versionOut, String &firmwareUrlOut);
  bool resolveHost(String &hostOut);
  bool downloadAndInstall(const String &url, const String &newVersion);
  static int compareVersions(const String &lhs, const String &rhs);

  unsigned long _lastCheckAt;
  bool _checkedSinceBoot;
};

