/*
 * Effects.h
 *
 * Collection of lighting effects for the addressable LED strip.
 */

#pragma once

#include "LedStrip.h"

class Effect {
public:
  virtual ~Effect() = default;

  virtual void begin(unsigned long now) = 0;
  virtual void update(unsigned long now) = 0;

  void attachStrip(LedStrip *strip, uint16_t ledCount);

protected:
  LedStrip &strip();
  uint16_t ledCount() const { return _ledCount; }

private:
  LedStrip *_strip = nullptr;
  uint16_t _ledCount = 0;
};

class SolidColorEffect : public Effect {
public:
  SolidColorEffect(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0);

  void setColor(uint8_t red, uint8_t green, uint8_t blue);

  void begin(unsigned long now) override;
  void update(unsigned long now) override;

private:
  uint8_t _red;
  uint8_t _green;
  uint8_t _blue;
  bool _dirty;
};

class SnakeEffect : public Effect {
public:
  SnakeEffect(uint8_t headRed = 0, uint8_t headGreen = 255, uint8_t headBlue = 0,
              uint8_t tailRed = 0, uint8_t tailGreen = 32, uint8_t tailBlue = 0,
              uint8_t backgroundRed = 0, uint8_t backgroundGreen = 0, uint8_t backgroundBlue = 0,
              unsigned long intervalMs = 75UL);

  void setHeadColor(uint8_t red, uint8_t green, uint8_t blue);
  void setTailColor(uint8_t red, uint8_t green, uint8_t blue);
  void setBackgroundColor(uint8_t red, uint8_t green, uint8_t blue);
  void setInterval(unsigned long intervalMs);

  void begin(unsigned long now) override;
  void update(unsigned long now) override;

private:
  void draw();

  uint8_t _headRed;
  uint8_t _headGreen;
  uint8_t _headBlue;
  uint8_t _tailRed;
  uint8_t _tailGreen;
  uint8_t _tailBlue;
  uint8_t _backgroundRed;
  uint8_t _backgroundGreen;
  uint8_t _backgroundBlue;
  unsigned long _intervalMs;
  unsigned long _lastStep;
  uint16_t _position;
};

class CometEffect : public Effect {
public:
  enum class Direction { Clockwise, CounterClockwise };

  CometEffect(uint8_t red = 255, uint8_t green = 255, uint8_t blue = 255,
              float firstTailFactor = 0.5f, float secondTailFactor = 0.2f,
              unsigned long intervalMs = 50UL,
              Direction direction = Direction::Clockwise);

  void setColor(uint8_t red, uint8_t green, uint8_t blue);
  void setTailFactors(float firstTailFactor, float secondTailFactor);
  void setInterval(unsigned long intervalMs);
  void setDirection(Direction direction);

  void begin(unsigned long now) override;
  void update(unsigned long now) override;

private:
  void draw();
  uint32_t scaledColor(float factor);

  uint8_t _red;
  uint8_t _green;
  uint8_t _blue;
  float _firstTailFactor;
  float _secondTailFactor;
  unsigned long _intervalMs;
  unsigned long _lastStep;
  uint16_t _position;
  Direction _direction;
};

class FadeEffect : public Effect {
public:
  FadeEffect(uint8_t startRed = 255, uint8_t startGreen = 0, uint8_t startBlue = 0,
             uint8_t endRed = 0, uint8_t endGreen = 0, uint8_t endBlue = 0,
             unsigned long durationMs = 300UL);

  void setColors(uint8_t startRed, uint8_t startGreen, uint8_t startBlue,
                 uint8_t endRed, uint8_t endGreen, uint8_t endBlue);
  void setDuration(unsigned long durationMs);

  void begin(unsigned long now) override;
  void update(unsigned long now) override;

private:
  void apply(float progress);

  uint8_t _startRed;
  uint8_t _startGreen;
  uint8_t _startBlue;
  uint8_t _endRed;
  uint8_t _endGreen;
  uint8_t _endBlue;
  unsigned long _durationMs;
  unsigned long _startTime;
  bool _complete;
};

class BreathingEffect : public Effect {
public:
  BreathingEffect(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 255, unsigned long periodMs = 2000UL);

  void setColor(uint8_t red, uint8_t green, uint8_t blue);
  void setPeriod(unsigned long periodMs);

  void begin(unsigned long now) override;
  void update(unsigned long now) override;

private:
  void applyIntensity(float intensity);

  uint8_t _red;
  uint8_t _green;
  uint8_t _blue;
  unsigned long _periodMs;
  unsigned long _startTime;
};

class RainbowEffect : public Effect {
public:
  RainbowEffect(unsigned long intervalMs = 20UL);

  void setInterval(unsigned long intervalMs);

  void begin(unsigned long now) override;
  void update(unsigned long now) override;

private:
  void draw();
  uint32_t wheel(uint8_t position);

  unsigned long _intervalMs;
  unsigned long _lastStep;
  uint8_t _offset;
};

