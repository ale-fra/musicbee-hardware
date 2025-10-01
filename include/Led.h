/*
 * Led.h
 *
 * RGB LED abstraction for controlling a 4-pin RGB LED module.
 * Supports different colors for success, error, and status indication.
 * Assumes Common Cathode configuration by default (set COMMON_ANODE to true for Common Anode).
 */

#pragma once

#include <Arduino.h>

class Led {
public:
  /**
   * Initialize the RGB LED. Pass the GPIO pins for Red, Green, Blue.
   * The common pin should be connected to GND (Common Cathode) or VCC (Common Anode).
   */
  void begin(uint8_t redPin, uint8_t greenPin, uint8_t bluePin, bool commonAnode = false);

  /**
   * Turn the LED off completely
   */
  void off();

  /**
   * Set a specific color (0-255 for each channel)
   */
  void setColor(uint8_t red, uint8_t green, uint8_t blue);

  /**
   * Set predefined colors
   */
  void setRed();
  void setGreen(); 
  void setBlue();
  void setYellow();
  void setPurple();
  void setCyan();
  void setWhite();

  /**
   * Blink a success pattern (green flashes)
   */
  void blinkSuccess();

  /**
   * Blink an error pattern (red flashes)  
   */
  void blinkError();

  /**
   * Blink a connecting pattern (blue flashes)
   */
  void blinkConnecting();

  /**
   * Show WiFi status (blue for connecting, green for connected, red for failed)
   */
  void showWifiStatus(bool connected, bool connecting = false);

private:
  uint8_t _redPin;
  uint8_t _greenPin; 
  uint8_t _bluePin;
  bool _commonAnode;

  void _writeColor(uint8_t red, uint8_t green, uint8_t blue);
};