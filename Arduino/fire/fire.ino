// Compact cellular fire effect for Matrix Portal M4.

#include "SignDisplay.h"

#define MATRIX_WIDTH SIGN_WIDTH
#define MATRIX_HEIGHT SIGN_HEIGHT
#define COOLING 42
#define SPARKING 130
#define FRAME_DELAY_MS 24

uint8_t heat[MATRIX_HEIGHT][MATRIX_WIDTH];

uint8_t clampSub(uint8_t value, uint8_t amount) {
  return value > amount ? value - amount : 0;
}

uint16_t heatColor(uint8_t temperature) {
  uint8_t t192 = (temperature * 191) / 255;
  uint8_t heatramp = (t192 & 0x3f) << 2;

  if (t192 > 0x80) {
    return matrix.color565(255, 255, heatramp);
  }
  if (t192 > 0x40) {
    return matrix.color565(255, heatramp, 0);
  }
  return matrix.color565(heatramp, 0, 0);
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));
  ProtomatterStatus status = matrix.begin();
  Serial.print("Protomatter begin() status: ");
  Serial.println((int)status);
  if (status != PROTOMATTER_OK) {
    return;
  }
}

void loop() {
  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      heat[y][x] = clampSub(heat[y][x], random(((COOLING * 10) / MATRIX_HEIGHT) + 2));
    }
  }

  for (int y = 0; y < MATRIX_HEIGHT - 2; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      int left = (x + MATRIX_WIDTH - 1) % MATRIX_WIDTH;
      int right = (x + 1) % MATRIX_WIDTH;
      heat[y][x] = (heat[y + 1][left] + heat[y + 1][x] + heat[y + 2][x] + heat[y + 1][right]) / 4;
    }
  }

  for (int x = 0; x < MATRIX_WIDTH; x++) {
    if (random(255) < SPARKING) {
      int y = MATRIX_HEIGHT - 1 - random(4);
      heat[y][x] = min(255, heat[y][x] + random(120, 255));
    }
  }

  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      matrix.drawPixel(x, y, heatColor(heat[y][x]));
    }
  }

  matrix.show();
  delay(FRAME_DELAY_MS);
}
