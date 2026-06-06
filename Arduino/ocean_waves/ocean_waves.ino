// Layered ocean wave and foam animation for Matrix Portal M4.
// Increase MATRIX_WIDTH to 128 or 192 when chaining more 64-pixel-wide panels.

#include <Adafruit_Protomatter.h>
#include <math.h>

uint8_t rgbPins[]  = {7, 8, 9, 10, 11, 12};
uint8_t addrPins[] = {17, 18, 19, 20, 21};
uint8_t clockPin   = 14;
uint8_t latchPin   = 15;
uint8_t oePin      = 16;

#define MATRIX_WIDTH 64
#define MATRIX_HEIGHT 64
#define FRAME_DELAY_MS 20

#if MATRIX_HEIGHT == 16
#define NUM_ADDR_PINS 3
#elif MATRIX_HEIGHT == 32
#define NUM_ADDR_PINS 4
#elif MATRIX_HEIGHT == 64
#define NUM_ADDR_PINS 5
#else
#error "MATRIX_HEIGHT must be 16, 32, or 64"
#endif

Adafruit_Protomatter matrix(
  MATRIX_WIDTH, 4, 1, rgbPins, NUM_ADDR_PINS, addrPins,
  clockPin, latchPin, oePin, false);

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
