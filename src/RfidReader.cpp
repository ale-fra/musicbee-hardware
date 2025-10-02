/*
 * RfidReader.cpp
 *
 * Backend-agnostic RFID/NFC reader façade. Selects the correct backend
 * implementation at compile time based on Config.h constants.
 */

#include "RfidReader.h"

#if defined(USE_RC522) && defined(USE_PN532)
#error "Only one of USE_RC522 or USE_PN532 may be defined"
#elif !defined(USE_RC522) && !defined(USE_PN532)
#error "One of USE_RC522 or USE_PN532 must be defined"
#endif

#if defined(USE_RC522)
#include <MFRC522.h>
#include <SPI.h>
#endif

#if defined(USE_PN532)
#include <Wire.h>
#include <Adafruit_PN532.h>
#endif

#include <array>

#include "Config.h"

namespace {

String bytesToHexString(const uint8_t *buffer, size_t length) {
  String uidHex;
  uidHex.reserve(length * 2);
  for (size_t i = 0; i < length; ++i) {
    if (buffer[i] < 0x10) {
      uidHex += '0';
    }
    uidHex += String(buffer[i], HEX);
  }
  uidHex.toUpperCase();
  return uidHex;
}

#if defined(USE_RC522)
class Rc522Backend final : public IRfidBackend {
public:
  Rc522Backend(uint8_t ssPin, uint8_t rstPin)
      : _ssPin(ssPin), _rstPin(rstPin), _mfrc522(ssPin, rstPin) {}

  bool begin() override {
    Serial.printf("[RFID] Initializing RC522 with SS=%d, RST=%d\n", _ssPin, _rstPin);

    SPI.begin();
    Serial.println("[RFID] SPI initialized");

    _mfrc522.PCD_Init();
    Serial.println("[RFID] MFRC522 initialized");

    byte version = _mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
    Serial.printf("[RFID] MFRC522 version: 0x%02X ", version);

    if (version == 0x00 || version == 0xFF) {
      Serial.println("- Communication problem or invalid chip!");
      Serial.println("[RFID] Check your wiring and power supply");
      return false;
    }

    Serial.println("- OK!");
    _mfrc522.PCD_DumpVersionToSerial();
    return true;
  }

  bool readCard(String &uidHex) override {
    if (!_mfrc522.PICC_IsNewCardPresent()) {
      return false;
    }

    Serial.println("[RFID] New card detected, attempting to read...");

    if (!_mfrc522.PICC_ReadCardSerial()) {
      Serial.println("[RFID] Failed to read card serial");
      return false;
    }

    Serial.printf("[RFID] Card read successfully, UID size: %d bytes\n", _mfrc522.uid.size);

    uidHex = bytesToHexString(_mfrc522.uid.uidByte, _mfrc522.uid.size);
    Serial.printf("[RFID] UID as hex string: %s\n", uidHex.c_str());

    MFRC522::PICC_Type piccType = _mfrc522.PICC_GetType(_mfrc522.uid.sak);
    Serial.printf("[RFID] Card type: %s\n", _mfrc522.PICC_GetTypeName(piccType));

    _mfrc522.PICC_HaltA();
    _mfrc522.PCD_StopCrypto1();
    return true;
  }

private:
  uint8_t  _ssPin;
  uint8_t  _rstPin;
  MFRC522 _mfrc522;
};
#endif  // defined(USE_RC522)

#if defined(USE_PN532)
class Pn532Backend final : public IRfidBackend {
public:
  Pn532Backend(uint8_t irqPin, uint8_t resetPin, uint8_t sdaPin, uint8_t sclPin)
      : _irqPin(irqPin),
        _resetPin(resetPin),
        _sdaPin(sdaPin),
        _sclPin(sclPin),
        _pn532(irqPin, resetPin) {}

  bool begin() override {
    Serial.printf("[RFID] Initializing PN532 (IRQ=%d, RST=%d, SDA=%d, SCL=%d)\n",
                  _irqPin, _resetPin, _sdaPin, _sclPin);

    Wire.begin(_sdaPin, _sclPin);
    Serial.println("[RFID] I2C bus initialized");

    _pn532.begin();

    uint32_t version = _pn532.getFirmwareVersion();
    if (!version) {
      Serial.println("[RFID] Failed to find PN532. Check wiring.");
      return false;
    }

    Serial.printf("[RFID] PN532 firmware: v%lu.%lu (0x%08lX)\n",
                  (version >> 24) & 0xFF, (version >> 16) & 0xFF, version);

    if (!_pn532.SAMConfig()) {
      Serial.println("[RFID] PN532 SAM configuration failed");
      return false;
    }

    Serial.println("[RFID] PN532 ready for passive reads");
    _initialised = true;
    return true;
  }

  bool readCard(String &uidHex) override {
    if (!_initialised) {
      return false;
    }

    std::array<uint8_t, 10> uid{};
    uint8_t uidLength = 0;

    bool success = _pn532.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid.data(), &uidLength, 1000);
    if (!success) {
      return false;
    }

    if (uidLength > uid.size()) {
      Serial.printf("[RFID] PN532 UID length %d exceeds buffer size %u, aborting read\n",
                    uidLength, static_cast<unsigned int>(uid.size()));
      return false;
    }

    Serial.printf("[RFID] PN532 detected card, UID length: %d bytes\n", uidLength);
    uidHex = bytesToHexString(uid.data(), uidLength);
    Serial.printf("[RFID] UID as hex string: %s\n", uidHex.c_str());
    return true;
  }

private:
  uint8_t          _irqPin;
  uint8_t          _resetPin;
  uint8_t          _sdaPin;
  uint8_t          _sclPin;
  Adafruit_PN532   _pn532;
  bool             _initialised = false;
};
#endif  // defined(USE_PN532)

}  // namespace

void RfidReader::begin() {
  switch (NFC_READER_TYPE) {
#if defined(USE_RC522)
    case RfidHardwareType::RC522:
      _backend.reset(new Rc522Backend(NFC_SS_PIN, NFC_RST_PIN));
      break;
#endif
#if defined(USE_PN532)
    case RfidHardwareType::PN532:
      _backend.reset(new Pn532Backend(PN532_IRQ_PIN, PN532_RST_PIN, PN532_SDA_PIN, PN532_SCL_PIN));
      break;
#endif
    default:
      Serial.println("[RFID] Unsupported reader type selected");
      _backend.reset();
      return;
  }

  if (_backend && !_backend->begin()) {
    Serial.println("[RFID] Backend initialisation failed");
    _backend.reset();
  }
}

bool RfidReader::readCard(String &uidHex) {
  if (!_backend) {
    Serial.println("[RFID] No reader backend is initialised");
    return false;
  }

  return _backend->readCard(uidHex);
}
