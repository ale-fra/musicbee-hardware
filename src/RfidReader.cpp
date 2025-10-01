/*
 * RfidReader.cpp
 *
 * Enhanced RfidReader implementation with debugging
 */

#include "RfidReader.h"

void RfidReader::begin(uint8_t ssPin, uint8_t rstPin) {
  Serial.printf("[RFID] Initializing with SS=%d, RST=%d\n", ssPin, rstPin);
  
  // Initialize SPI
  SPI.begin();
  Serial.println("[RFID] SPI initialized");
  
  // Create MFRC522 instance
  _mfrc522 = MFRC522(ssPin, rstPin);
  
  // Initialize the MFRC522
  _mfrc522.PCD_Init(ssPin, rstPin);
  Serial.println("[RFID] MFRC522 initialized");
  
  // Test if we can communicate with the chip
  byte version = _mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.printf("[RFID] MFRC522 version: 0x%02X ", version);
  
  if (version == 0x00 || version == 0xFF) {
    Serial.println("- Communication problem or invalid chip!");
    Serial.println("[RFID] Check your wiring and power supply");
  } else {
    Serial.println("- OK!");
    // Show some details of the PCD - MFRC522 Card Reader
    _mfrc522.PCD_DumpVersionToSerial();
  }
}

bool RfidReader::readCard(String &uidHex) {
  // Look for new cards
  if (!_mfrc522.PICC_IsNewCardPresent()) {
    return false; // No card present, this is normal
  }
  
  Serial.println("[RFID] New card detected, attempting to read...");
  
  if (!_mfrc522.PICC_ReadCardSerial()) {
    Serial.println("[RFID] Failed to read card serial");
    return false;
  }
  
  Serial.printf("[RFID] Card read successfully, UID size: %d bytes\n", _mfrc522.uid.size);
  
  // Convert UID bytes to uppercase hex string
  uidHex = "";
  Serial.print("[RFID] UID bytes: ");
  for (byte i = 0; i < _mfrc522.uid.size; ++i) {
    byte val = _mfrc522.uid.uidByte[i];
    Serial.printf("0x%02X ", val);
    
    if (val < 0x10) {
      uidHex += '0';
    }
    uidHex += String(val, HEX);
  }
  Serial.println();
  uidHex.toUpperCase();
  
  Serial.printf("[RFID] UID as hex string: %s\n", uidHex.c_str());
  
  // Show card type
  MFRC522::PICC_Type piccType = _mfrc522.PICC_GetType(_mfrc522.uid.sak);
  Serial.printf("[RFID] Card type: %s\n", _mfrc522.PICC_GetTypeName(piccType));
  
  // Halt the PICC and stop encryption so other tags can be read
  _mfrc522.PICC_HaltA();
  _mfrc522.PCD_StopCrypto1();
  
  return true;
}