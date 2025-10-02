#include "Effects.h"

#include <math.h>

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

void Effect::attachStrip(LedStrip *strip, uint16_t ledCount) {
  _strip = strip;
  _ledCount = ledCount;
}

LedStrip &Effect::strip() {
  return *_strip;
}

SolidColorEffect::SolidColorEffect(uint8_t red, uint8_t green, uint8_t blue)
    : _red(red), _green(green), _blue(blue), _dirty(true) {}

void SolidColorEffect::setColor(uint8_t red, uint8_t green, uint8_t blue) {
  _red = red;
  _green = green;
  _blue = blue;
  _dirty = true;
}

void SolidColorEffect::begin(unsigned long /*now*/) {
  _dirty = true;
}

void SolidColorEffect::update(unsigned long /*now*/) {
  if (!_dirty) {
    return;
  }

  uint32_t colorValue = strip().color(_red, _green, _blue);
  strip().setAll(colorValue);
  strip().apply();
  _dirty = false;
}

SnakeEffect::SnakeEffect(uint8_t headRed, uint8_t headGreen, uint8_t headBlue,
                         uint8_t tailRed, uint8_t tailGreen, uint8_t tailBlue,
                         uint8_t backgroundRed, uint8_t backgroundGreen, uint8_t backgroundBlue,
                         unsigned long intervalMs)
    : _headRed(headRed),
      _headGreen(headGreen),
      _headBlue(headBlue),
      _tailRed(tailRed),
      _tailGreen(tailGreen),
      _tailBlue(tailBlue),
      _backgroundRed(backgroundRed),
      _backgroundGreen(backgroundGreen),
      _backgroundBlue(backgroundBlue),
      _intervalMs(intervalMs),
      _lastStep(0),
      _position(0) {}

void SnakeEffect::setHeadColor(uint8_t red, uint8_t green, uint8_t blue) {
  _headRed = red;
  _headGreen = green;
  _headBlue = blue;
}

void SnakeEffect::setTailColor(uint8_t red, uint8_t green, uint8_t blue) {
  _tailRed = red;
  _tailGreen = green;
  _tailBlue = blue;
}

void SnakeEffect::setBackgroundColor(uint8_t red, uint8_t green, uint8_t blue) {
  _backgroundRed = red;
  _backgroundGreen = green;
  _backgroundBlue = blue;
}

void SnakeEffect::setInterval(unsigned long intervalMs) {
  _intervalMs = intervalMs;
}

void SnakeEffect::begin(unsigned long now) {
  _position = 0;
  _lastStep = now;
  draw();
}

void SnakeEffect::update(unsigned long now) {
  if (now - _lastStep < _intervalMs) {
    return;
  }

  _lastStep = now;
  _position = (_position + 1) % ledCount();
  draw();
}

void SnakeEffect::draw() {
  uint32_t background = strip().color(_backgroundRed, _backgroundGreen, _backgroundBlue);
  uint32_t head = strip().color(_headRed, _headGreen, _headBlue);
  uint32_t tail = strip().color(_tailRed, _tailGreen, _tailBlue);

  strip().setAll(background);

  if (ledCount() == 0) {
    strip().apply();
    return;
  }

  uint16_t tailIndex = (_position + ledCount() - 1) % ledCount();
  strip().setPixel(tailIndex, tail);
  strip().setPixel(_position, head);
  strip().apply();
}

BreathingEffect::BreathingEffect(uint8_t red, uint8_t green, uint8_t blue, unsigned long periodMs)
    : _red(red), _green(green), _blue(blue), _periodMs(periodMs), _startTime(0) {}

void BreathingEffect::setColor(uint8_t red, uint8_t green, uint8_t blue) {
  _red = red;
  _green = green;
  _blue = blue;
}

void BreathingEffect::setPeriod(unsigned long periodMs) {
  _periodMs = periodMs == 0 ? 1 : periodMs;
}

void BreathingEffect::begin(unsigned long now) {
  _startTime = now;
  applyIntensity(0.0f);
}

void BreathingEffect::update(unsigned long now) {
  unsigned long elapsed = now - _startTime;
  unsigned long period = _periodMs == 0 ? 1 : _periodMs;
  float phase = static_cast<float>(elapsed % period) / static_cast<float>(period);
  float intensity = 0.5f * (1.0f - cosf(phase * 2.0f * static_cast<float>(M_PI)));
  applyIntensity(intensity);
}

void BreathingEffect::applyIntensity(float intensity) {
  auto scale = [](uint8_t base, float factor) -> uint8_t {
    float value = static_cast<float>(base) * factor;
    if (value <= 0.0f) {
      return 0;
    }
    if (value >= 255.0f) {
      return 255;
    }
    return static_cast<uint8_t>(value + 0.5f);
  };

  uint8_t r = scale(_red, intensity);
  uint8_t g = scale(_green, intensity);
  uint8_t b = scale(_blue, intensity);

  uint32_t colorValue = strip().color(r, g, b);
  strip().setAll(colorValue);
  strip().apply();
}

