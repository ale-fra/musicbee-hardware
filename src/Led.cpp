/*
 * Led.cpp
 *
 * RGB LED implementation with color support and various blink patterns
 */

#include "Led.h"

void Led::begin(uint8_t redPin, uint8_t greenPin, uint8_t bluePin, bool commonAnode) {
  _redPin = redPin;
  _greenPin = greenPin;
  _bluePin = bluePin;
  _commonAnode = commonAnode;

  pinMode(_redPin, OUTPUT);
  pinMode(_greenPin, OUTPUT);
  pinMode(_bluePin, OUTPUT);
  
  off(); // Start with LED off
  
  Serial.printf("[LED] RGB LED initialized - R:%d G:%d B:%d, Common %s\n", 
                _redPin, _greenPin, _bluePin, _commonAnode ? "Anode" : "Cathode");
}

void Led::_writeColor(uint8_t red, uint8_t green, uint8_t blue) {
  if (_commonAnode) {
    // Common Anode: LOW = ON, HIGH = OFF
    analogWrite(_redPin, 255 - red);
    analogWrite(_greenPin, 255 - green);
    analogWrite(_bluePin, 255 - blue);
  } else {
    // Common Cathode: HIGH = ON, LOW = OFF
    analogWrite(_redPin, red);
    analogWrite(_greenPin, green);
    analogWrite(_bluePin, blue);
  }
}

void Led::off() {
  _writeColor(0, 0, 0);
}

void Led::setColor(uint8_t red, uint8_t green, uint8_t blue) {
  _writeColor(red, green, blue);
}

void Led::setRed() {
  _writeColor(255, 0, 0);
}

void Led::setGreen() {
  _writeColor(0, 255, 0);
}

void Led::setBlue() {
  _writeColor(0, 0, 255);
}

void Led::setYellow() {
  _writeColor(255, 255, 0);
}

void Led::setPurple() {
  _writeColor(255, 0, 255);
}

void Led::setCyan() {
  _writeColor(0, 255, 255);
}

void Led::setWhite() {
  _writeColor(255, 255, 255);
}

void Led::blinkSuccess() {
  Serial.println("[LED] Success pattern - Green blinks");
  for (uint8_t i = 0; i < 3; ++i) {
    setGreen();
    delay(150);
    off();
    delay(150);
  }
}

void Led::blinkError() {
  Serial.println("[LED] Error pattern - Red blinks");
  for (uint8_t i = 0; i < 3; ++i) {
    setRed();
    delay(300);
    off();
    delay(200);
  }
}

void Led::blinkConnecting() {
  Serial.println("[LED] Connecting pattern - Blue pulse");
  for (uint8_t i = 0; i < 2; ++i) {
    setBlue();
    delay(100);
    off();
    delay(100);
  }
}

void Led::showWifiStatus(bool connected, bool connecting) {
  if (connecting) {
    blinkConnecting();
  } else if (connected) {
    setGreen();
    delay(500);
    off();
  } else {
    setRed();
    delay(500);
    off();
  }
}