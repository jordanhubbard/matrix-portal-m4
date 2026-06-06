#pragma once

#include "Arduino.h"

#define NEO_GRB 0x01
#define NEO_KHZ800 0x02

class Adafruit_NeoPixel {
public:
  Adafruit_NeoPixel(uint16_t count, int16_t pin, uint8_t type = NEO_GRB + NEO_KHZ800)
      : count_(count) {
    (void)pin;
    (void)type;
  }

  void begin() {}

  void clear() {
    for (uint16_t i = 0; i < count_; ++i) {
      MatrixPortalSim::setStatusPixel(i, 0, 0, 0);
    }
  }

  void show() {}

  void setBrightness(uint8_t brightness) {
    brightness_ = brightness;
  }

  void setPixelColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (brightness_ < 255) {
      r = static_cast<uint8_t>((static_cast<uint16_t>(r) * brightness_) / 255);
      g = static_cast<uint8_t>((static_cast<uint16_t>(g) * brightness_) / 255);
      b = static_cast<uint8_t>((static_cast<uint16_t>(b) * brightness_) / 255);
    }
    MatrixPortalSim::setStatusPixel(index, r, g, b);
  }

  void setPixelColor(uint16_t index, uint32_t color) {
    setPixelColor(index,
                  static_cast<uint8_t>((color >> 16) & 0xff),
                  static_cast<uint8_t>((color >> 8) & 0xff),
                  static_cast<uint8_t>(color & 0xff));
  }

  uint32_t Color(uint8_t r, uint8_t g, uint8_t b) const {
    return (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) |
           static_cast<uint32_t>(b);
  }

private:
  uint16_t count_;
  uint8_t brightness_ = 255;
};
