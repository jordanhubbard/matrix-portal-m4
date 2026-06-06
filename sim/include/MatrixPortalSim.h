#pragma once

#include <cstdint>

namespace MatrixPortalSim {
struct Rgb {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
};

struct HardwareState {
  float accelX = 0.0f;
  float accelY = 0.0f;
  float accelZ = 9.8f;
  bool autoTilt = true;
  bool buttonUpPressed = false;
  bool buttonDownPressed = false;
  bool d13Led = false;
  Rgb statusPixel;
  int analogValues[5] = {512, 512, 512, 512, 512};
};

HardwareState &hardware();
void setStatusPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
}
