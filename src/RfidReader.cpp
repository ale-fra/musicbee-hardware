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
#  include <Adafruit_PN532.h>
#  if defined(USE_PN532_SPI)
#    include <SPI.h>
#  else
#    include <Wire.h>
#  endif
#endif

#include <array>

#include "Config.h"

namespace {

#if defined(USE_PN532)
static constexpr unsigned long PN532_ASYNC_RESTART_DELAY_MS = 5;
static constexpr unsigned long PN532_ASYNC_RESPONSE_TIMEOUT_MS = 75;
static constexpr unsigned long PN532_POST_BUS_DELAY_MS = 100;
static constexpr unsigned long PN532_POST_BEGIN_DELAY_MS = 100;
static constexpr unsigned long PN532_FIRMWARE_RETRY_DELAY_MS = 500;
static constexpr uint8_t       PN532_FIRMWARE_MAX_ATTEMPTS = 3;
#endif

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
    if (_initialised) {
      return true;
    }

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
      _initialisationFailed = true;
      return false;
    }

    Serial.println("- OK!");
    _mfrc522.PCD_DumpVersionToSerial();
    _initialised = true;
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

  bool isReady() const override {
    return _initialised;
  }

  bool hasFailed() const override {
    return _initialisationFailed;
  }

private:
  uint8_t  _ssPin;
  uint8_t  _rstPin;
  MFRC522 _mfrc522;
  bool     _initialised = false;
  bool     _initialisationFailed = false;
};
#endif  // defined(USE_RC522)

#if defined(USE_PN532)
class Pn532Backend final : public IRfidBackend {
public:
#  if defined(USE_PN532_SPI)
  Pn532Backend(uint8_t irqPin,
               uint8_t resetPin,
               uint8_t ssPin,
               uint8_t sckPin,
               uint8_t mosiPin,
               uint8_t misoPin)
      : _irqPin(irqPin),
        _resetPin(resetPin),
        _ssPin(ssPin),
        _sckPin(sckPin),
        _mosiPin(mosiPin),
        _misoPin(misoPin),
        _pn532(ssPin) {}
#  else
  Pn532Backend(uint8_t irqPin, uint8_t resetPin, uint8_t sdaPin, uint8_t sclPin)
      : _irqPin(irqPin),
        _resetPin(resetPin),
        _sdaPin(sdaPin),
        _sclPin(sclPin),
        _pn532(irqPin, resetPin) {}
#  endif

  bool begin() override {
    if (_initialised) {
      return true;
    }

    if (_initialisationFailed) {
      return false;
    }

    const unsigned long now = millis();

    switch (_beginState) {
      case BeginState::Idle:
#  if defined(USE_PN532_SPI)
        Serial.printf("[RFID] Initializing PN532 SPI (IRQ=%d, RST=%d, SS=%d, SCK=%d, MOSI=%d, MISO=%d)\n",
                      _irqPin, _resetPin, _ssPin, _sckPin, _mosiPin, _misoPin);
        SPI.begin(_sckPin, _misoPin, _mosiPin, _ssPin);
        Serial.println("[RFID] SPI bus initialized");
#  else
        Serial.printf("[RFID] Initializing PN532 I2C (IRQ=%d, RST=%d, SDA=%d, SCL=%d)\n",
                      _irqPin, _resetPin, _sdaPin, _sclPin);
        Wire.begin(_sdaPin, _sclPin);
        Serial.println("[RFID] I2C bus initialized");
#  endif
        _beginState = BeginState::WaitingAfterBusInit;
        _stateEnteredAt = now;
        return false;
      case BeginState::WaitingAfterBusInit:
        if (now - _stateEnteredAt < PN532_POST_BUS_DELAY_MS) {
          return false;
        }
        Serial.println("[RFID] Calling PN532 begin()...");
        _pn532.begin();
        _beginState = BeginState::WaitingAfterBegin;
        _stateEnteredAt = now;
        return false;
      case BeginState::WaitingAfterBegin:
        if (now - _stateEnteredAt < PN532_POST_BEGIN_DELAY_MS) {
          return false;
        }
        Serial.println("[RFID] Attempting to get firmware version...");
        _beginState = BeginState::FirmwareQuery;
        return false;
      case BeginState::FirmwareQuery: {
        if (_firmwareAttempts >= PN532_FIRMWARE_MAX_ATTEMPTS) {
          logFirmwareFailure();
          _initialisationFailed = true;
          _beginState = BeginState::Failed;
          return false;
        }

        if (_firmwareAttempts > 0 &&
            now - _lastFirmwareAttemptMs < PN532_FIRMWARE_RETRY_DELAY_MS) {
          return false;
        }

        uint8_t attempt = _firmwareAttempts + 1;
        Serial.printf("[RFID] Firmware version attempt %d/%d...\n",
                      attempt, PN532_FIRMWARE_MAX_ATTEMPTS);

        uint32_t version = _pn532.getFirmwareVersion();
        _lastFirmwareAttemptMs = now;
        _firmwareAttempts++;

        if (!version) {
          if (_firmwareAttempts < PN532_FIRMWARE_MAX_ATTEMPTS) {
            Serial.println("[RFID] No response, retrying...");
          }
          return false;
        }

        Serial.printf("[RFID] PN532 firmware: v%lu.%lu (0x%08lX)\n",
                      (version >> 24) & 0xFF, (version >> 16) & 0xFF, version);
        Serial.println("[RFID] Configuring SAM...");

        if (!_pn532.SAMConfig()) {
          Serial.println("[RFID] PN532 SAM configuration failed");
          _initialisationFailed = true;
          _beginState = BeginState::Failed;
          return false;
        }

        Serial.println("[RFID] PN532 ready for passive reads");
        _initialised = true;
        _beginState = BeginState::Ready;
        return true;
      }
      case BeginState::Ready:
        return true;
      case BeginState::Failed:
        return false;
    }

    return false;
  }

  bool readCard(String &uidHex) override {
    if (!_initialised) {
      return false;
    }

    const unsigned long now = millis();

    if (!_awaitingPassiveTarget) {
      if (now - _lastDetectionCommandMs < PN532_ASYNC_RESTART_DELAY_MS) {
        return false;
      }

      if (!_pn532.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A)) {
        if (!_loggedStartFailure) {
          Serial.println("[RFID] PN532 failed to start passive target detection");
          _loggedStartFailure = true;
        }
        _lastDetectionCommandMs = now;
        return false;
      }

      _awaitingPassiveTarget = true;
      _lastDetectionCommandMs = now;
      _loggedStartFailure = false;
      return false;
    }

    std::array<uint8_t, 10> uid{};
    uint8_t uidLength = 0;

    bool success = _pn532.readDetectedPassiveTargetID(uid.data(), &uidLength);

    if (!success) {
      if (now - _lastDetectionCommandMs >= PN532_ASYNC_RESPONSE_TIMEOUT_MS) {
        _awaitingPassiveTarget = false;
        _lastDetectionCommandMs = now;
      }
      return false;
    }

    _awaitingPassiveTarget = false;
    _lastDetectionCommandMs = now;

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

  bool isReady() const override {
    return _initialised;
  }

  bool hasFailed() const override {
    return _initialisationFailed;
  }

private:
  enum class BeginState {
    Idle,
    WaitingAfterBusInit,
    WaitingAfterBegin,
    FirmwareQuery,
    Ready,
    Failed
  };

  void logFirmwareFailure() {
    if (_firmwareFailureLogged) {
      return;
    }
    Serial.println("[RFID] Failed to find PN532 after 3 attempts.");
    Serial.println("[RFID] Troubleshooting checklist:");
    Serial.println("[RFID]   1. Check DIP switches: SW1=OFF, SW2=ON for SPI mode");
    Serial.println("[RFID]   2. Verify wiring matches Config.h pin definitions");
    Serial.println("[RFID]   3. Ensure PN532 is powered with 3.3V (NOT 5V)");
    Serial.println("[RFID]   4. Check for loose connections");
    Serial.println("[RFID]   5. Try with shorter wires (<20cm)");
    _firmwareFailureLogged = true;
  }

  uint8_t _irqPin;
  uint8_t _resetPin;
#  if defined(USE_PN532_SPI)
  uint8_t        _ssPin;
  uint8_t        _sckPin;
  uint8_t        _mosiPin;
  uint8_t        _misoPin;
#  else
  uint8_t        _sdaPin;
  uint8_t        _sclPin;
#  endif
  Adafruit_PN532 _pn532;
  bool           _initialised = false;
  bool           _awaitingPassiveTarget = false;
  unsigned long  _lastDetectionCommandMs = 0;
  bool           _loggedStartFailure = false;
  BeginState     _beginState = BeginState::Idle;
  unsigned long  _stateEnteredAt = 0;
  unsigned long  _lastFirmwareAttemptMs = 0;
  uint8_t        _firmwareAttempts = 0;
  bool           _initialisationFailed = false;
  bool           _firmwareFailureLogged = false;
};
#endif  // defined(USE_PN532)

}  // namespace

void RfidReader::begin() {
  if (_backendReady || _backendFailed) {
    return;
  }

  if (!_backend) {
    switch (NFC_READER_TYPE) {
#if defined(USE_RC522)
      case RfidHardwareType::RC522:
        _backend.reset(new Rc522Backend(NFC_SS_PIN, NFC_RST_PIN));
        break;
#endif
#if defined(USE_PN532)
      case RfidHardwareType::PN532:
        _backend.reset(new Pn532Backend(PN532_IRQ_PIN,
                                        PN532_RST_PIN,
#  if defined(USE_PN532_SPI)
                                        PN532_SS_PIN,
                                        PN532_SCK_PIN,
                                        PN532_MOSI_PIN,
                                        PN532_MISO_PIN
#  else
                                        PN532_SDA_PIN,
                                        PN532_SCL_PIN
#  endif
                                        ));
        break;
#endif
      default:
        Serial.println("[RFID] Unsupported reader type selected");
        _backend.reset();
        _backendFailed = true;
        return;
    }
  }

  if (!_backend) {
    return;
  }

  if (_backend->begin()) {
    _backendReady = true;
    return;
  }

  if (_backend->hasFailed()) {
    Serial.println("[RFID] Backend initialisation failed");
    _backend.reset();
    _backendFailed = true;
  }
}

bool RfidReader::readCard(String &uidHex) {
  if (!_backendReady) {
    begin();
    if (!_backendReady) {
      return false;
    }
  }

  if (!_backend) {
    Serial.println("[RFID] No reader backend is initialised");
    return false;
  }

  return _backend->readCard(uidHex);
}
