#pragma once

#include <Adafruit_Protomatter.h>
#include <Arduino.h>
#include "PanelLayout.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef TWO_PI
#define TWO_PI 6.28318530717958647692
#endif

#ifndef SIGN_PANEL_COUNT
#define SIGN_PANEL_COUNT PANEL_LAYOUT_PANEL_COUNT
#endif

#ifndef SIGN_PANEL_WIDTH
#define SIGN_PANEL_WIDTH PANEL_LAYOUT_PANEL_WIDTH
#endif

#ifndef SIGN_PANEL_HEIGHT
#define SIGN_PANEL_HEIGHT PANEL_LAYOUT_PANEL_HEIGHT
#endif

#ifndef PANEL_COUNT
#define PANEL_COUNT PANEL_LAYOUT_PANEL_COUNT
#endif

#ifndef PANEL_WIDTH
#define PANEL_WIDTH PANEL_LAYOUT_PANEL_WIDTH
#endif

#ifndef PANEL_HEIGHT
#define PANEL_HEIGHT PANEL_LAYOUT_PANEL_HEIGHT
#endif

#define SIGN_RAW_WIDTH PANEL_LAYOUT_SOURCE_WIDTH
#define SIGN_RAW_HEIGHT PANEL_LAYOUT_SOURCE_HEIGHT
#define SIGN_WIDTH PANEL_LAYOUT_PHYSICAL_WIDTH
#define SIGN_HEIGHT PANEL_LAYOUT_PHYSICAL_HEIGHT

#if SIGN_RAW_HEIGHT == 16
#define SIGN_NUM_ADDR_PINS 3
#elif SIGN_RAW_HEIGHT == 32
#define SIGN_NUM_ADDR_PINS 4
#elif SIGN_RAW_HEIGHT == 64
#define SIGN_NUM_ADDR_PINS 5
#else
#error "PANEL_LAYOUT_SOURCE_HEIGHT must be 16, 32, or 64"
#endif

uint8_t rgbPins[]  = {7, 8, 9, 10, 11, 12};
uint8_t addrPins[] = {17, 18, 19, 20, 21};
uint8_t clockPin   = 14;
uint8_t latchPin   = 15;
uint8_t oePin      = 16;

inline int signPanelRotatedWidth(const MatrixPortalPanelLayoutEntry &panel) {
  return panel.rotation == 90 || panel.rotation == 270 ? panel.height : panel.width;
}

inline int signPanelRotatedHeight(const MatrixPortalPanelLayoutEntry &panel) {
  return panel.rotation == 90 || panel.rotation == 270 ? panel.width : panel.height;
}

inline bool signMapPixelToSource(int x, int y, int16_t &sourceX, int16_t &sourceY) {
  for (int i = 0; i < PANEL_LAYOUT_PANEL_COUNT; i++) {
    const MatrixPortalPanelLayoutEntry &panel = PANEL_LAYOUT[i];
    int localX = x - panel.x;
    int localY = y - panel.y;
    if (localX < 0 || localY < 0 ||
        localX >= signPanelRotatedWidth(panel) ||
        localY >= signPanelRotatedHeight(panel)) {
      continue;
    }

    switch (panel.rotation) {
    case 90:
      sourceX = panel.sourceX + localY;
      sourceY = panel.sourceY + panel.height - 1 - localX;
      break;
    case 180:
      sourceX = panel.sourceX + panel.width - 1 - localX;
      sourceY = panel.sourceY + panel.height - 1 - localY;
      break;
    case 270:
      sourceX = panel.sourceX + panel.width - 1 - localY;
      sourceY = panel.sourceY + localX;
      break;
    default:
      sourceX = panel.sourceX + localX;
      sourceY = panel.sourceY + localY;
      break;
    }
    return sourceX >= 0 && sourceY >= 0 &&
           sourceX < SIGN_RAW_WIDTH && sourceY < SIGN_RAW_HEIGHT;
  }
  return false;
}

class SignDisplayMatrix {
public:
  SignDisplayMatrix() :
    hardware(SIGN_RAW_WIDTH, 4, 1, rgbPins, SIGN_NUM_ADDR_PINS, addrPins,
             clockPin, latchPin, oePin, false) {
  }

  ProtomatterStatus begin() {
    return hardware.begin();
  }

  int16_t width() const {
    return SIGN_WIDTH;
  }

  int16_t height() const {
    return SIGN_HEIGHT;
  }

  uint16_t color565(uint8_t red, uint8_t green, uint8_t blue) {
    return hardware.color565(red, green, blue);
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) {
    int16_t sourceX = 0;
    int16_t sourceY = 0;
    if (signMapPixelToSource(x, y, sourceX, sourceY)) {
      hardware.drawPixel(sourceX, sourceY, color);
    }
  }

  void fillScreen(uint16_t color) {
    hardware.fillScreen(color);
  }

  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    drawLine(x, y, x + w - 1, y, color);
    drawLine(x, y + h - 1, x + w - 1, y + h - 1, color);
    drawLine(x, y, x, y + h - 1, color);
    drawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    for (int16_t yy = y; yy < y + h; yy++) {
      for (int16_t xx = x; xx < x + w; xx++) {
        drawPixel(xx, yy, color);
      }
    }
  }

  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t dx = abs(x1 - x0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = -abs(y1 - y0);
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy;

    while (true) {
      drawPixel(x0, y0, color);
      if (x0 == x1 && y0 == y1) {
        break;
      }
      int16_t e2 = 2 * err;
      if (e2 >= dy) {
        err += dy;
        x0 += sx;
      }
      if (e2 <= dx) {
        err += dx;
        y0 += sy;
      }
    }
  }

  void drawCircle(int16_t x0, int16_t y0, int16_t radius, uint16_t color) {
    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;
    while (x >= y) {
      drawPixel(x0 + x, y0 + y, color);
      drawPixel(x0 + y, y0 + x, color);
      drawPixel(x0 - y, y0 + x, color);
      drawPixel(x0 - x, y0 + y, color);
      drawPixel(x0 - x, y0 - y, color);
      drawPixel(x0 - y, y0 - x, color);
      drawPixel(x0 + y, y0 - x, color);
      drawPixel(x0 + x, y0 - y, color);
      if (err <= 0) {
        y++;
        err += 2 * y + 1;
      }
      if (err > 0) {
        x--;
        err -= 2 * x + 1;
      }
    }
  }

  void fillCircle(int16_t x0, int16_t y0, int16_t radius, uint16_t color) {
    for (int16_t y = -radius; y <= radius; y++) {
      for (int16_t x = -radius; x <= radius; x++) {
        if (x * x + y * y <= radius * radius) {
          drawPixel(x0 + x, y0 + y, color);
        }
      }
    }
  }

  void drawTriangle(int16_t x0, int16_t y0,
                    int16_t x1, int16_t y1,
                    int16_t x2, int16_t y2,
                    uint16_t color) {
    drawLine(x0, y0, x1, y1, color);
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x0, y0, color);
  }

  void show() {
    hardware.show();
  }

  uint32_t getFrameCount() {
    return hardware.getFrameCount();
  }

  void println(const char *text) {
    Serial.println(text);
  }

private:
  Adafruit_Protomatter hardware;
};

SignDisplayMatrix matrix;

inline uint8_t signClampByte(int value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return (uint8_t)value;
}

inline uint16_t signRgb(uint8_t r, uint8_t g, uint8_t b) {
  return matrix.color565(r, g, b);
}

inline uint16_t signWheel(uint8_t pos) {
  pos = 255 - pos;
  if (pos < 85) {
    return signRgb(255 - pos * 3, 0, pos * 3);
  }
  if (pos < 170) {
    pos -= 85;
    return signRgb(0, pos * 3, 255 - pos * 3);
  }
  pos -= 170;
  return signRgb(pos * 3, 255 - pos * 3, 0);
}

inline const char **signGlyph(char c) {
  static const char *space[] = {"00000","00000","00000","00000","00000","00000","00000"};
  static const char *unknown[] = {"11111","10001","00010","00100","00100","00000","00100"};
  static const char *digits[][7] = {
    {"01110","10001","10011","10101","11001","10001","01110"},
    {"00100","01100","00100","00100","00100","00100","01110"},
    {"01110","10001","00001","00010","00100","01000","11111"},
    {"11110","00001","00001","01110","00001","00001","11110"},
    {"00010","00110","01010","10010","11111","00010","00010"},
    {"11111","10000","11110","00001","00001","10001","01110"},
    {"00110","01000","10000","11110","10001","10001","01110"},
    {"11111","00001","00010","00100","01000","01000","01000"},
    {"01110","10001","10001","01110","10001","10001","01110"},
    {"01110","10001","10001","01111","00001","00010","01100"}
  };
  static const char *letters[][7] = {
    {"01110","10001","10001","11111","10001","10001","10001"},
    {"11110","10001","10001","11110","10001","10001","11110"},
    {"01110","10001","10000","10000","10000","10001","01110"},
    {"11110","10001","10001","10001","10001","10001","11110"},
    {"11111","10000","10000","11110","10000","10000","11111"},
    {"11111","10000","10000","11110","10000","10000","10000"},
    {"01110","10001","10000","10111","10001","10001","01110"},
    {"10001","10001","10001","11111","10001","10001","10001"},
    {"01110","00100","00100","00100","00100","00100","01110"},
    {"00111","00010","00010","00010","00010","10010","01100"},
    {"10001","10010","10100","11000","10100","10010","10001"},
    {"10000","10000","10000","10000","10000","10000","11111"},
    {"10001","11011","10101","10101","10001","10001","10001"},
    {"10001","11001","10101","10011","10001","10001","10001"},
    {"01110","10001","10001","10001","10001","10001","01110"},
    {"11110","10001","10001","11110","10000","10000","10000"},
    {"01110","10001","10001","10001","10101","10010","01101"},
    {"11110","10001","10001","11110","10100","10010","10001"},
    {"01111","10000","10000","01110","00001","00001","11110"},
    {"11111","00100","00100","00100","00100","00100","00100"},
    {"10001","10001","10001","10001","10001","10001","01110"},
    {"10001","10001","10001","10001","10001","01010","00100"},
    {"10001","10001","10001","10101","10101","10101","01010"},
    {"10001","10001","01010","00100","01010","10001","10001"},
    {"10001","10001","01010","00100","00100","00100","00100"},
    {"11111","00001","00010","00100","01000","10000","11111"}
  };
  static const char *dash[] = {"00000","00000","00000","11111","00000","00000","00000"};
  static const char *plus[] = {"00000","00100","00100","11111","00100","00100","00000"};
  static const char *slash[] = {"00001","00010","00010","00100","01000","01000","10000"};
  static const char *dot[] = {"00000","00000","00000","00000","00000","01100","01100"};
  static const char *colon[] = {"00000","01100","01100","00000","01100","01100","00000"};
  static const char *percent[] = {"11001","11010","00100","01000","10110","00110","00000"};

  if (c >= 'a' && c <= 'z') {
    c = c - 'a' + 'A';
  }
  if (c == ' ') return space;
  if (c >= '0' && c <= '9') return digits[c - '0'];
  if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
  switch (c) {
  case '-': return dash;
  case '+': return plus;
  case '/': return slash;
  case '.': return dot;
  case ':': return colon;
  case '%': return percent;
  default: return unknown;
  }
}

inline int signTextWidth(const char *text, int scale = 1) {
  return text ? (int)strlen(text) * 6 * scale : 0;
}

inline void signDrawChar(int x, int y, char c, uint16_t color, int scale = 1) {
  const char **rows = signGlyph(c);
  for (int row = 0; row < 7; row++) {
    for (int col = 0; col < 5; col++) {
      if (rows[row][col] == '1') {
        if (scale == 1) {
          matrix.drawPixel(x + col, y + row, color);
        } else {
          matrix.fillRect(x + col * scale, y + row * scale, scale, scale, color);
        }
      }
    }
  }
}

inline void signDrawText(int x, int y, const char *text, uint16_t color, int scale = 1) {
  if (!text) return;
  while (*text) {
    signDrawChar(x, y, *text, color, scale);
    x += 6 * scale;
    text++;
  }
}

inline void signDrawCenteredText(int y, const char *text, uint16_t color, int scale = 1) {
  signDrawText((SIGN_WIDTH - signTextWidth(text, scale)) / 2, y, text, color, scale);
}

inline bool signBegin() {
  Serial.begin(115200);
  ProtomatterStatus status = matrix.begin();
  Serial.print("Protomatter begin() status: ");
  Serial.println((int)status);
  return status == PROTOMATTER_OK;
}
