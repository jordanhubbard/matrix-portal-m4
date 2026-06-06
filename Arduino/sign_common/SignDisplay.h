#pragma once

#include <Adafruit_Protomatter.h>
#include <Arduino.h>
#include <math.h>
#include <string.h>

#ifndef TWO_PI
#define TWO_PI 6.28318530717958647692
#endif

#ifndef SIGN_PANEL_COUNT
#define SIGN_PANEL_COUNT 4
#endif

#ifndef SIGN_PANEL_WIDTH
#define SIGN_PANEL_WIDTH 32
#endif

#ifndef SIGN_PANEL_HEIGHT
#define SIGN_PANEL_HEIGHT 32
#endif

#define SIGN_WIDTH (SIGN_PANEL_COUNT * SIGN_PANEL_WIDTH)
#define SIGN_HEIGHT SIGN_PANEL_HEIGHT

#if SIGN_HEIGHT == 16
#define SIGN_NUM_ADDR_PINS 3
#elif SIGN_HEIGHT == 32
#define SIGN_NUM_ADDR_PINS 4
#elif SIGN_HEIGHT == 64
#define SIGN_NUM_ADDR_PINS 5
#else
#error "SIGN_PANEL_HEIGHT must be 16, 32, or 64"
#endif

uint8_t rgbPins[]  = {7, 8, 9, 10, 11, 12};
uint8_t addrPins[] = {17, 18, 19, 20, 21};
uint8_t clockPin   = 14;
uint8_t latchPin   = 15;
uint8_t oePin      = 16;

Adafruit_Protomatter matrix(
  SIGN_WIDTH, 4, 1, rgbPins, SIGN_NUM_ADDR_PINS, addrPins,
  clockPin, latchPin, oePin, false);

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
