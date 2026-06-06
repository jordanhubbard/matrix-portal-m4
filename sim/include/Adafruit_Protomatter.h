#pragma once

#include "Arduino.h"

#include <cstdint>
#include <string>
#include <vector>

enum ProtomatterStatus {
  PROTOMATTER_OK = 0,
  PROTOMATTER_ERR = 1
};

class Adafruit_Protomatter {
public:
  Adafruit_Protomatter(int16_t width, uint8_t bitDepth, uint8_t numChains,
                       uint8_t *rgbPins, uint8_t numAddrPins,
                       uint8_t *addrPins, uint8_t clockPin,
                       uint8_t latchPin, uint8_t oePin, bool doubleBuffer);

  ProtomatterStatus begin();
  int16_t width() const;
  int16_t height() const;
  uint16_t color565(uint8_t red, uint8_t green, uint8_t blue) const;
  void drawPixel(int16_t x, int16_t y, uint16_t color);
  void fillScreen(uint16_t color);
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  void drawCircle(int16_t x0, int16_t y0, int16_t radius, uint16_t color);
  void fillCircle(int16_t x0, int16_t y0, int16_t radius, uint16_t color);
  void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                    int16_t x2, int16_t y2, uint16_t color);
  void println(const char *text);
  void show();
  uint32_t getFrameCount() const;

  const std::vector<uint16_t> &pixels() const;

private:
  int16_t width_;
  int16_t height_;
  uint32_t frameCount_ = 0;
  std::vector<uint16_t> pixels_;
};

namespace MatrixPortalSim {
void configureFromArgs(int argc, char **argv);
bool running();
void pumpEvents();
void present(const Adafruit_Protomatter &matrix);
void shutdown();
void sleep(unsigned long ms);
}
