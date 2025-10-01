/*
 * RfidReader.h
 *
 * Abstraction around the MFRC522 RFID/NFC reader. It handles
 * initialisation and reading the UID of presented tags. After reading
 * a card the reader is halted to allow further detection. See
 * RfidReader.cpp for implementation details.
 */

#pragma once

#include <Arduino.h>
#include <memory>

class IRfidBackend {
public:
  virtual ~IRfidBackend() = default;
  virtual bool begin() = 0;
  virtual bool readCard(String &uidHex) = 0;
};

class RfidReader {
public:
  /**
   * Initialise the configured RFID/NFC backend. Pin assignments and
   * hardware type are defined in Config.h.
   */
  void begin();

  /**
   * Attempt to read a card UID. Returns true if a new card is
   * present and the UID has been populated into `uidHex`. The UID
   * string will be uppercase hexadecimal without separators.
   */
  bool readCard(String &uidHex);

private:
  std::unique_ptr<IRfidBackend> _backend;
};