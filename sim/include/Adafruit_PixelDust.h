#pragma once

#include "Arduino.h"

#include <algorithm>
#include <vector>

using dimension_t = int16_t;

class Adafruit_PixelDust {
public:
  Adafruit_PixelDust(int width, int height, int grains, int, int, bool)
      : width_(width), height_(height), grains_(grains), particles_(grains) {}

  bool begin() {
    return true;
  }

  void randomize() {
    for (auto &particle : particles_) {
      particle.x = static_cast<double>(random(width_));
      particle.y = static_cast<double>(random(height_));
      particle.vx = 0.0;
      particle.vy = 0.0;
    }
  }

  void setPosition(int index, dimension_t x, dimension_t y) {
    if (index < 0 || index >= grains_) {
      return;
    }
    particles_[index].x = x;
    particles_[index].y = y;
    particles_[index].vx = 0.0;
    particles_[index].vy = 0.0;
  }

  void getPosition(int index, dimension_t *x, dimension_t *y) const {
    if (index < 0 || index >= grains_) {
      *x = 0;
      *y = 0;
      return;
    }
    *x = static_cast<dimension_t>(std::clamp<int>(static_cast<int>(particles_[index].x), 0, width_ - 1));
    *y = static_cast<dimension_t>(std::clamp<int>(static_cast<int>(particles_[index].y), 0, height_ - 1));
  }

  void iterate(double ax, double ay, double az) {
    (void)az;
    const double gx = ax * 0.00003;
    const double gy = ay * 0.00003;

    for (auto &particle : particles_) {
      particle.vx = std::clamp(particle.vx + gx, -1.5, 1.5);
      particle.vy = std::clamp(particle.vy + gy, -1.5, 1.5);
      particle.x += particle.vx;
      particle.y += particle.vy;

      if (particle.x < 0 || particle.x >= width_) {
        particle.vx *= -0.45;
        particle.x = std::clamp(particle.x, 0.0, static_cast<double>(width_ - 1));
      }
      if (particle.y < 0 || particle.y >= height_) {
        particle.vy *= -0.45;
        particle.y = std::clamp(particle.y, 0.0, static_cast<double>(height_ - 1));
      }
    }
  }

private:
  struct Particle {
    double x = 0.0;
    double y = 0.0;
    double vx = 0.0;
    double vy = 0.0;
  };

  int width_;
  int height_;
  int grains_;
  std::vector<Particle> particles_;
};
