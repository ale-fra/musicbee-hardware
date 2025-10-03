#include "EffectManager.h"

#include <Arduino.h>

EffectManager::EffectManager(uint8_t dataPin, uint16_t ledCount, uint8_t defaultBrightness)
    : _strip(dataPin, ledCount),
      _ledCount(ledCount),
      _brightness(defaultBrightness),
      _activeEffect(nullptr),
      _solidEffect(),
      _snakeEffect(),
      _breathingEffect() {}

void EffectManager::begin(unsigned long now) {
  (void)now;
  _strip.begin();
  _strip.setBrightness(_brightness);
  _strip.setAll(_strip.color(0, 0, 0));
  _strip.apply();
  _activeEffect = nullptr;
  Serial.printf("[Effects] LED strip initialised with %u LEDs at brightness %u\n",
                _ledCount, _brightness);
}

void EffectManager::update(unsigned long now) {
  if (_activeEffect == nullptr) {
    return;
  }
  _activeEffect->update(now);
}

void EffectManager::setEffect(Effect &effect, unsigned long now) {
  activateEffect(effect, now);
}

void EffectManager::showSolidColor(uint8_t red, uint8_t green, uint8_t blue, unsigned long now) {
  _solidEffect.setColor(red, green, blue);
  Serial.printf("[Effects] Activating SolidColor (R:%u G:%u B:%u)\n", red, green, blue);
  activateEffect(_solidEffect, now);
}

void EffectManager::showSnake(uint8_t red, uint8_t green, uint8_t blue, unsigned long now) {
  _snakeEffect.setHeadColor(red, green, blue);
  _snakeEffect.setTailColor(red / 6, green / 6, blue / 6);
  _snakeEffect.setBackgroundColor(0, 0, 0);
  Serial.printf("[Effects] Activating Snake (head:%u,%u,%u) at %lums\n",
                red, green, blue, now);
  activateEffect(_snakeEffect, now);
}

void EffectManager::showBreathing(uint8_t red, uint8_t green, uint8_t blue, unsigned long now) {
  _breathingEffect.setColor(red, green, blue);
  Serial.printf("[Effects] Activating Breathing (R:%u G:%u B:%u) at %lums\n",
                red, green, blue, now);
  activateEffect(_breathingEffect, now);
}

void EffectManager::setBrightness(uint8_t brightness) {
  _brightness = brightness;
  _strip.setBrightness(brightness);
  if (_activeEffect != nullptr) {
    Serial.printf("[Effects] Brightness changed to %u - forcing immediate update\n", brightness);
    _activeEffect->update(millis());
  }
}

void EffectManager::activateEffect(Effect &effect, unsigned long now) {
  effect.attachStrip(&_strip, _ledCount);
  _activeEffect = &effect;
  Serial.printf("[Effects] Effect initialised at %lums\n", now);
  _activeEffect->begin(now);
  _activeEffect->update(now);
}

