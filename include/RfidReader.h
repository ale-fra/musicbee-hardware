/*
 * RfidReader.h
 *
 * Abstraction around the MFRC522 RFID/NFC reader. It handles
 * initialisation and reading the UID of presented tags. After reading
 * a card the reader is halted to allow further detection. See
 * RfidReader.cpp for implementation details.
 */

#pragma once

#include <MFRC522.h>
#include <SPI.h>

class RfidReader {
public:
  /**
   * Initialise the RC522 reader. Provide the SPI SS and reset pins.
   */
  void begin(uint8_t ssPin, uint8_t rstPin);

  /**
   * Attempt to read a card UID. Returns true if a new card is
   * present and the UID has been populated into `uidHex`. The UID
   * string will be uppercase hexadecimal without separators.
   */
  bool readCard(String &uidHex);

private:
  MFRC522 _mfrc522;
};