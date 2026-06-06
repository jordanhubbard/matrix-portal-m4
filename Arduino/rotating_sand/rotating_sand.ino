// Tilt-reactive rotating sand bowl for Matrix Portal M4.
// Uses the onboard LIS3DH accelerometer. The SDL simulator supplies synthetic tilt.

#include <Adafruit_Protomatter.h>
#include <Adafruit_LIS3DH.h>
#include <Wire.h>
#include <math.h>

uint8_t rgbPins[]  = {7, 8, 9, 10, 11, 12};
uint8_t addrPins[] = {17, 18, 19, 20, 21};
uint8_t clockPin   = 14;
uint8_t latchPin   = 15;
uint8_t oePin      = 16;

#define MATRIX_WIDTH 64
#define MATRIX_HEIGHT 64
#define GRAINS 430
#define MAX_FPS 45

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
  clockPin, latchPin, oePin, true);
Adafruit_LIS3DH accel = Adafruit_LIS3DH();

struct Grain {
  float x;
  float y;
  float vx;
  float vy;
};

Grain grains[GRAINS];
uint32_t prevTime = 0;

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return matrix.color565(r, g, b);
}

uint8_t clampByte(int value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return (uint8_t)value;
}

void seedSand() {
  float cx = (MATRIX_WIDTH - 1) * 0.5f;
  float cy = (MATRIX_HEIGHT - 1) * 0.5f;
  float radius = min(MATRIX_WIDTH, MATRIX_HEIGHT) * 0.40f;

  for (int i = 0; i < GRAINS; i++) {
    float a = random(6283) * 0.001f;
    float r = sqrtf(random(10000) * 0.0001f) * radius * 0.82f;
    grains[i].x = cx + cosf(a) * r;
    grains[i].y = cy + sinf(a) * r + radius * 0.20f;
    grains[i].vx = 0.0f;
    grains[i].vy = 0.0f;
  }
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

  if (!accel.begin(0x19)) {
    Serial.println("Couldn't find accelerometer");
  } else {
    accel.setRange(LIS3DH_RANGE_4_G);
  }

  seedSand();
}

void loop() {
  uint32_t t;
  while (((t = micros()) - prevTime) < (1000000UL / MAX_FPS)) {
  }
  prevTime = t;

  sensors_event_t event;
  accel.getEvent(&event);
  float gx = event.acceleration.x * 0.028f;
  float gy = event.acceleration.y * 0.028f;

  float cx = (MATRIX_WIDTH - 1) * 0.5f;
  float cy = (MATRIX_HEIGHT - 1) * 0.5f;
  float radius = min(MATRIX_WIDTH, MATRIX_HEIGHT) * 0.45f;
  float radius2 = radius * radius;

  for (int i = 0; i < GRAINS; i++) {
    grains[i].vx = (grains[i].vx + gx) * 0.985f;
    grains[i].vy = (grains[i].vy + gy) * 0.985f;
    grains[i].x += grains[i].vx;
    grains[i].y += grains[i].vy;

    float dx = grains[i].x - cx;
    float dy = grains[i].y - cy;
    float d2 = dx * dx + dy * dy;
    if (d2 > radius2) {
      float d = sqrtf(d2);
      float nx = dx / d;
      float ny = dy / d;
      grains[i].x = cx + nx * radius;
      grains[i].y = cy + ny * radius;

      float dot = grains[i].vx * nx + grains[i].vy * ny;
      if (dot > 0.0f) {
        grains[i].vx -= dot * nx * 1.55f;
        grains[i].vy -= dot * ny * 1.55f;
      }
      grains[i].vx *= 0.72f;
      grains[i].vy *= 0.72f;
    }
  }

  matrix.fillScreen(0);
  matrix.drawCircle((int)cx, (int)cy, (int)radius, rgb(20, 32, 46));
  matrix.drawCircle((int)cx, (int)cy, (int)(radius - 1), rgb(8, 12, 18));

  for (int i = 0; i < GRAINS; i++) {
    int speed = (int)((fabsf(grains[i].vx) + fabsf(grains[i].vy)) * 55.0f);
    uint16_t color = rgb(clampByte(208 + speed), clampByte(150 + speed / 2), clampByte(52 + speed / 5));
    matrix.drawPixel((int)grains[i].x, (int)grains[i].y, color);
  }

  matrix.show();
}
