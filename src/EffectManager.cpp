#include "EffectManager.h"

#include <Arduino.h>

EffectManager::EffectManager(uint8_t dataPin, uint16_t ledCount, uint8_t defaultBrightness)
    : _strip(dataPin, ledCount),
      _ledCount(ledCount),
      _brightness(defaultBrightness),
      _activeEffect(nullptr),
      _solidEffect(),
      _snakeEffect(),
      _breathingEffect(),
      _cometEffect(),
      _fadeEffect(),
      _rainbowEffect() {}

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

void EffectManager::showComet(uint8_t red, uint8_t green, uint8_t blue,
                              float firstTailFactor, float secondTailFactor,
                              CometEffect::Direction direction,
                              unsigned long intervalMs, unsigned long now) {
  _cometEffect.setColor(red, green, blue);
  _cometEffect.setTailFactors(firstTailFactor, secondTailFactor);
  _cometEffect.setDirection(direction);
  _cometEffect.setInterval(intervalMs);
  Serial.printf("[Effects] Activating Comet (R:%u G:%u B:%u, tail %.2f/%.2f, %s, %lums)\n",
                red,
                green,
                blue,
                firstTailFactor,
                secondTailFactor,
                direction == CometEffect::Direction::Clockwise ? "CW" : "CCW",
                intervalMs);
  activateEffect(_cometEffect, now);
}

void EffectManager::showFade(uint8_t startRed, uint8_t startGreen, uint8_t startBlue,
                             uint8_t endRed, uint8_t endGreen, uint8_t endBlue,
                             unsigned long durationMs, unsigned long now) {
  _fadeEffect.setColors(startRed, startGreen, startBlue, endRed, endGreen, endBlue);
  _fadeEffect.setDuration(durationMs);
  Serial.printf("[Effects] Activating Fade (from %u,%u,%u to %u,%u,%u over %lums)\n",
                startRed,
                startGreen,
                startBlue,
                endRed,
                endGreen,
                endBlue,
                durationMs);
  activateEffect(_fadeEffect, now);
}

void EffectManager::showRainbow(unsigned long intervalMs, unsigned long now) {
  _rainbowEffect.setInterval(intervalMs);
  Serial.printf("[Effects] Activating Rainbow (interval %lums)\n", intervalMs);
  activateEffect(_rainbowEffect, now);
}

void EffectManager::turnOff(unsigned long now) {
  uint32_t off = _strip.color(0, 0, 0);
  _strip.setAll(off);
  _strip.apply();
  _activeEffect = nullptr;
  Serial.printf("[Effects] Strip turned off at %lums\n", now);
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

