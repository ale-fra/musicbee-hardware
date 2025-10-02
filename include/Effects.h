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

