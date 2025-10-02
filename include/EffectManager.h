/*
 * EffectManager.h
 *
 * Coordinates lighting effects for the WS2812 LED strip.
 */

#pragma once

#include "Effects.h"

class EffectManager {
public:
  EffectManager(uint8_t dataPin, uint16_t ledCount, uint8_t defaultBrightness);

  void begin(unsigned long now = 0);
  void update(unsigned long now);

  void setEffect(Effect &effect, unsigned long now);

  void showSolidColor(uint8_t red, uint8_t green, uint8_t blue, unsigned long now);
  void showSnake(uint8_t red, uint8_t green, uint8_t blue, unsigned long now);
  void showBreathing(uint8_t red, uint8_t green, uint8_t blue, unsigned long now);

  void setBrightness(uint8_t brightness);
  uint8_t brightness() const { return _brightness; }

  LedStrip &strip() { return _strip; }
  uint16_t ledCount() const { return _ledCount; }

  SolidColorEffect &solidEffect() { return _solidEffect; }
  SnakeEffect &snakeEffect() { return _snakeEffect; }
  BreathingEffect &breathingEffect() { return _breathingEffect; }

private:
  void activateEffect(Effect &effect, unsigned long now);

  LedStrip _strip;
  uint16_t _ledCount;
  uint8_t _brightness;
  Effect *_activeEffect;
  SolidColorEffect _solidEffect;
  SnakeEffect _snakeEffect;
  BreathingEffect _breathingEffect;
};

