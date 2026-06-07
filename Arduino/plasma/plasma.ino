// Classic low-resolution plasma effect for RGB matrix panels.
// Good on 32x32, 64x64, or wider chained displays.

#include "SignDisplay.h"
#include <math.h>

#define MATRIX_WIDTH SIGN_WIDTH
#define MATRIX_HEIGHT SIGN_HEIGHT
#define FRAME_DELAY_MS 16

uint16_t hsvToRgb565(uint8_t h, uint8_t s, uint8_t v) {
  uint8_t region = h / 43;
  uint8_t remainder = (h - (region * 43)) * 6;

  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

  uint8_t r, g, b;
  switch (region) {
  case 0: r = v; g = t; b = p; break;
  case 1: r = q; g = v; b = p; break;
  case 2: r = p; g = v; b = t; break;
  case 3: r = p; g = q; b = v; break;
  case 4: r = t; g = p; b = v; break;
  default: r = v; g = p; b = q; break;
  }
  return matrix.color565(r, g, b);
}

void setup() {
  Serial.begin(115200);
  ProtomatterStatus status = matrix.begin();
  Serial.print("Protomatter begin() status: ");
  Serial.println((int)status);
  if (status != PROTOMATTER_OK) {
    return;
  }
}

void loop() {
  float t = millis() * 0.002f;
  float cx = MATRIX_WIDTH * 0.5f + sinf(t * 0.7f) * MATRIX_WIDTH * 0.20f;
  float cy = MATRIX_HEIGHT * 0.5f + cosf(t * 0.9f) * MATRIX_HEIGHT * 0.18f;

  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      float dx = x - cx;
      float dy = y - cy;
      float value =
        sinf(x * 0.21f + t * 2.5f) +
        sinf(y * 0.18f - t * 2.0f) +
        sinf((x + y) * 0.11f + t * 1.7f) +
        sinf(sqrtf(dx * dx + dy * dy) * 0.25f - t * 3.4f);
      uint8_t hue = (uint8_t)((value + 4.0f) * 31.0f + t * 32.0f);
      uint8_t val = (uint8_t)(145 + (sinf(value * 1.4f + t) + 1.0f) * 55.0f);
      matrix.drawPixel(x, y, hsvToRgb565(hue, 255, val));
    }
  }

  matrix.show();
  delay(FRAME_DELAY_MS);
}
