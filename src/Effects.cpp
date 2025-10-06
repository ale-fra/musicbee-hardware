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
  uint16_t count = ledCount();
  if (count == 0) {
    _lastStep = now;
    draw();
    return;
  }

  unsigned long interval = _intervalMs == 0 ? 1 : _intervalMs;
  unsigned long elapsed = now - _lastStep;
  if (elapsed < interval) {
    return;
  }

  unsigned long steps = elapsed / interval;
  if (steps == 0) {
    steps = 1;
  }

  _lastStep += steps * interval;
  uint16_t stepCount = static_cast<uint16_t>(steps % count);
  if (stepCount == 0) {
    stepCount = count;
  }
  _position = (_position + stepCount) % count;
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

CometEffect::CometEffect(uint8_t red, uint8_t green, uint8_t blue,
                         float firstTailFactor, float secondTailFactor,
                         unsigned long intervalMs, Direction direction)
    : _red(red),
      _green(green),
      _blue(blue),
      _firstTailFactor(firstTailFactor),
      _secondTailFactor(secondTailFactor),
      _intervalMs(intervalMs),
      _lastStep(0),
      _position(0),
      _direction(direction) {}

void CometEffect::setColor(uint8_t red, uint8_t green, uint8_t blue) {
  _red = red;
  _green = green;
  _blue = blue;
}

void CometEffect::setTailFactors(float firstTailFactor, float secondTailFactor) {
  _firstTailFactor = firstTailFactor;
  _secondTailFactor = secondTailFactor;
}

void CometEffect::setInterval(unsigned long intervalMs) {
  _intervalMs = intervalMs == 0 ? 1 : intervalMs;
}

void CometEffect::setDirection(Direction direction) {
  _direction = direction;
}

void CometEffect::begin(unsigned long now) {
  _position = 0;
  _lastStep = now;
  draw();
}

void CometEffect::update(unsigned long now) {
  uint16_t count = ledCount();
  if (count == 0) {
    _lastStep = now;
    draw();
    return;
  }

  unsigned long interval = _intervalMs == 0 ? 1 : _intervalMs;
  unsigned long elapsed = now - _lastStep;
  if (elapsed < interval) {
    return;
  }

  unsigned long steps = elapsed / interval;
  if (steps == 0) {
    steps = 1;
  }

  _lastStep += steps * interval;
  uint16_t stepCount = static_cast<uint16_t>(steps % count);
  if (stepCount == 0) {
    stepCount = count;
  }
  if (_direction == Direction::Clockwise) {
    _position = (_position + stepCount) % count;
  } else {
    _position = (_position + count - stepCount) % count;
  }
  draw();
}

void CometEffect::draw() {
  strip().setAll(strip().color(0, 0, 0));

  if (ledCount() == 0) {
    strip().apply();
    return;
  }

  uint32_t head = strip().color(_red, _green, _blue);
  strip().setPixel(_position, head);

  if (ledCount() > 1) {
    uint16_t firstTailIndex = 0;
    if (_direction == Direction::Clockwise) {
      firstTailIndex = (_position + ledCount() - 1) % ledCount();
    } else {
      firstTailIndex = (_position + 1) % ledCount();
    }
    strip().setPixel(firstTailIndex, scaledColor(_firstTailFactor));
  }

  if (ledCount() > 2) {
    uint16_t secondTailIndex = 0;
    if (_direction == Direction::Clockwise) {
      secondTailIndex = (_position + ledCount() - 2) % ledCount();
    } else {
      secondTailIndex = (_position + 2) % ledCount();
    }
    strip().setPixel(secondTailIndex, scaledColor(_secondTailFactor));
  }

  strip().apply();
}

uint32_t CometEffect::scaledColor(float factor) {
  float clamped = factor;
  if (clamped < 0.0f) {
    clamped = 0.0f;
  } else if (clamped > 1.0f) {
    clamped = 1.0f;
  }

  auto scale = [clamped](uint8_t component) -> uint8_t {
    float value = static_cast<float>(component) * clamped;
    if (value <= 0.0f) {
      return 0;
    }
    if (value >= 255.0f) {
      return 255;
    }
    return static_cast<uint8_t>(value + 0.5f);
  };

  return strip().color(scale(_red), scale(_green), scale(_blue));
}

FadeEffect::FadeEffect(uint8_t startRed, uint8_t startGreen, uint8_t startBlue,
                       uint8_t endRed, uint8_t endGreen, uint8_t endBlue,
                       unsigned long durationMs)
    : _startRed(startRed),
      _startGreen(startGreen),
      _startBlue(startBlue),
      _endRed(endRed),
      _endGreen(endGreen),
      _endBlue(endBlue),
      _durationMs(durationMs),
      _startTime(0),
      _complete(false) {}

void FadeEffect::setColors(uint8_t startRed, uint8_t startGreen, uint8_t startBlue,
                           uint8_t endRed, uint8_t endGreen, uint8_t endBlue) {
  _startRed = startRed;
  _startGreen = startGreen;
  _startBlue = startBlue;
  _endRed = endRed;
  _endGreen = endGreen;
  _endBlue = endBlue;
}

void FadeEffect::setDuration(unsigned long durationMs) {
  _durationMs = durationMs;
}

void FadeEffect::begin(unsigned long now) {
  _startTime = now;
  _complete = false;
  apply(0.0f);
}

void FadeEffect::update(unsigned long now) {
  if (_complete && _durationMs != 0) {
    return;
  }

  if (_durationMs == 0) {
    apply(1.0f);
    _complete = true;
    return;
  }

  unsigned long elapsed = now - _startTime;
  float progress = static_cast<float>(elapsed) / static_cast<float>(_durationMs);
  if (progress >= 1.0f) {
    apply(1.0f);
    _complete = true;
  } else {
    apply(progress);
  }
}

void FadeEffect::apply(float progress) {
  float clamped = progress;
  if (clamped < 0.0f) {
    clamped = 0.0f;
  } else if (clamped > 1.0f) {
    clamped = 1.0f;
  }

  auto lerp = [clamped](uint8_t start, uint8_t end) -> uint8_t {
    float value = static_cast<float>(start) + (static_cast<float>(end) - static_cast<float>(start)) * clamped;
    if (value <= 0.0f) {
      return 0;
    }
    if (value >= 255.0f) {
      return 255;
    }
    return static_cast<uint8_t>(value + 0.5f);
  };

  uint8_t red = lerp(_startRed, _endRed);
  uint8_t green = lerp(_startGreen, _endGreen);
  uint8_t blue = lerp(_startBlue, _endBlue);

  uint32_t colorValue = strip().color(red, green, blue);
  strip().setAll(colorValue);
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

namespace {
constexpr float kBreathingMinIntensity = 0.1f;
constexpr float kBreathingRange = 1.0f - kBreathingMinIntensity;
}

void BreathingEffect::begin(unsigned long now) {
  _startTime = now;
  applyIntensity(kBreathingMinIntensity);
}

void BreathingEffect::update(unsigned long now) {
  unsigned long elapsed = now - _startTime;
  unsigned long period = _periodMs == 0 ? 1 : _periodMs;
  float phase = static_cast<float>(elapsed % period) / static_cast<float>(period);
  float intensity = 0.5f * (1.0f - cosf(phase * 2.0f * static_cast<float>(M_PI)));
  float adjustedIntensity = kBreathingMinIntensity + (kBreathingRange * intensity);
  applyIntensity(adjustedIntensity);
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

