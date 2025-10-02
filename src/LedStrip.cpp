#include "LedStrip.h"

LedStrip::LedStrip(uint8_t dataPin, uint16_t ledCount, neoPixelType pixelType)
    : _dataPin(dataPin),
      _ledCount(ledCount),
      _pixelType(pixelType),
      _strip(ledCount, dataPin, pixelType),
      _begun(false) {}

void LedStrip::begin() {
  if (_begun) {
    return;
  }

  _strip.begin();
  _strip.clear();
  _strip.show();
  _begun = true;
}

void LedStrip::setAll(uint32_t color) {
  if (!_begun) {
    return;
  }
  for (uint16_t i = 0; i < _ledCount; ++i) {
    _strip.setPixelColor(i, color);
  }
}

void LedStrip::setPixel(uint16_t index, uint32_t color) {
  if (!_begun || index >= _ledCount) {
    return;
  }
  _strip.setPixelColor(index, color);
}

void LedStrip::apply() {
  if (!_begun) {
    return;
  }
  _strip.show();
}

void LedStrip::setBrightness(uint8_t brightness) {
  if (!_begun) {
    return;
  }
  _strip.setBrightness(brightness);
}

uint32_t LedStrip::color(uint8_t red, uint8_t green, uint8_t blue) {
  return _strip.Color(red, green, blue);
}

