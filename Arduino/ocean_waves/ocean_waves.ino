#include "SignDisplay.h"
#include <math.h>

#define MATRIX_WIDTH SIGN_WIDTH
#define MATRIX_HEIGHT SIGN_HEIGHT
#define FRAME_DELAY_MS 20

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return matrix.color565(r, g, b);
}

uint8_t clampByte(int value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return (uint8_t)value;
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
  float t = millis() * 0.0018f;
  int horizon = MATRIX_HEIGHT / 3;

  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      float nx = (float)x;
      float ny = (float)y;
      float swell =
        sinf(nx * 0.115f + t * 2.0f) * 4.2f +
        sinf(nx * 0.047f + ny * 0.090f + t * 1.1f) * 5.6f +
        sinf(nx * 0.210f - t * 3.1f) * 1.8f;
      float surface = horizon + MATRIX_HEIGHT * 0.22f + swell;

      if (ny < surface) {
        int sky = (int)(28 + ny * 1.6f);
        matrix.drawPixel(x, y, rgb(0, clampByte(sky / 2), clampByte(36 + sky)));
        continue;
      }

      float depth = ny - surface;
      float caustic =
        sinf(nx * 0.42f + t * 4.2f + sinf(ny * 0.15f + t)) +
        sinf((nx + ny) * 0.20f - t * 3.5f);
      int foam = depth < 1.8f ? (int)((1.8f - depth) * 86.0f) : 0;
      int shimmer = caustic > 1.05f ? (int)((caustic - 1.05f) * 70.0f) : 0;
      int deep = (int)(depth * 3.2f);

      uint8_t r = clampByte(foam + shimmer / 3);
      uint8_t g = clampByte(54 + foam + shimmer - deep / 3);
      uint8_t b = clampByte(112 + foam + shimmer + deep);
      matrix.drawPixel(x, y, rgb(r, g, b));
    }
  }

  matrix.show();
  delay(FRAME_DELAY_MS);
}
