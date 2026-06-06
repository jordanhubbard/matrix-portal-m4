#pragma once

#include "Arduino.h"

#define LIS3DH_RANGE_4_G 4

struct sensors_vec_t {
  float x;
  float y;
  float z;
};

struct sensors_event_t {
  sensors_vec_t acceleration;
};

class Adafruit_LIS3DH {
public:
  bool begin(uint8_t address = 0x19) {
    (void)address;
    return true;
  }

  void setRange(int range) {
    (void)range;
  }

  void getEvent(sensors_event_t *event) {
    const auto &state = MatrixPortalSim::hardware();
    event->acceleration.x = state.accelX;
    event->acceleration.y = state.accelY;
    event->acceleration.z = state.accelZ;
  }
};
