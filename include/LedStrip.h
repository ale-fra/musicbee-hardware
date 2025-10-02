/*
 * LedStrip.h
 *
 * Lightweight wrapper around an addressable WS2812/NeoPixel LED strip.
 *
 * Wiring notes:
 *   - Connect strip VCC to a stable 5 V supply that can provide the required current.
 *   - Tie strip GND to the ESP32 ground.
 *   - Route the strip DIN (data) line to LED_DATA_PIN and insert a ~330 Ω resistor in series.
 *   - Place an optional 1000 µF capacitor across the strip's 5 V and GND rails to smooth inrush current.
 */

#pragma once

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

class LedStrip {
public:
  LedStrip(uint8_t dataPin, uint16_t ledCount, neoPixelType pixelType = NEO_GRB + NEO_KHZ800);

  void begin();
  void setAll(uint32_t color);
  void setPixel(uint16_t index, uint32_t color);
  void apply();
  void setBrightness(uint8_t brightness);

  uint16_t size() const { return _ledCount; }
  uint32_t color(uint8_t red, uint8_t green, uint8_t blue);

private:
  uint8_t _dataPin;
  uint16_t _ledCount;
  neoPixelType _pixelType;
  Adafruit_NeoPixel _strip;
  bool _begun;
};

