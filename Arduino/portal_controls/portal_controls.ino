// Matrix Portal M4 built-in device demo.
// Tilt controls the color field. Buttons 2/3 change status color and D13 state.

#include <Adafruit_Protomatter.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <math.h>

uint8_t rgbPins[]  = {7, 8, 9, 10, 11, 12};
uint8_t addrPins[] = {17, 18, 19, 20, 21};
uint8_t clockPin   = 14;
uint8_t latchPin   = 15;
uint8_t oePin      = 16;

#define MATRIX_WIDTH 64
#define MATRIX_HEIGHT 64
#define BUTTON_UP_PIN 2
#define BUTTON_DOWN_PIN 3
#define STATUS_NEOPIXEL_PIN 4
#define FRAME_DELAY_MS 25

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
Adafruit_LIS3DH accel = Adafruit_LIS3DH();
Adafruit_NeoPixel statusPixel(1, STATUS_NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

uint8_t paletteMode = 0;
bool ledOn = false;
bool lastUp = false;
bool lastDown = false;

uint8_t clampByte(int value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return (uint8_t)value;
}

uint16_t colorFor(int x, int y, float ax, float ay) {
  float wave = sinf((x + ax * 3.0f) * 0.15f) + cosf((y + ay * 3.0f) * 0.16f);
  int level = (int)((wave + 2.0f) * 63.0f);

  if (paletteMode == 1) {
    return matrix.color565(clampByte(level / 3), clampByte(40 + level), clampByte(170 + level / 3));
  }
  if (paletteMode == 2) {
    return matrix.color565(clampByte(190 + level / 4), clampByte(50 + level / 3), clampByte(level / 5));
  }
  return matrix.color565(clampByte(30 + level / 2), clampByte(120 + level / 3), clampByte(60 + level / 2));
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);

  statusPixel.begin();
  statusPixel.setBrightness(48);
  statusPixel.setPixelColor(0, statusPixel.Color(0, 40, 10));
  statusPixel.show();

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
}

void loop() {
  bool upPressed = digitalRead(BUTTON_UP_PIN) == LOW;
  bool downPressed = digitalRead(BUTTON_DOWN_PIN) == LOW;

  if (upPressed && !lastUp) {
    paletteMode = (paletteMode + 1) % 3;
  }
  if (downPressed && !lastDown) {
    ledOn = !ledOn;
  }
  lastUp = upPressed;
  lastDown = downPressed;

  digitalWrite(LED_BUILTIN, ledOn ? HIGH : LOW);
  if (paletteMode == 0) {
    statusPixel.setPixelColor(0, statusPixel.Color(0, 80, 20));
  } else if (paletteMode == 1) {
    statusPixel.setPixelColor(0, statusPixel.Color(0, 40, 120));
  } else {
    statusPixel.setPixelColor(0, statusPixel.Color(140, 28, 0));
  }
  statusPixel.show();

  sensors_event_t event;
  accel.getEvent(&event);

  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      matrix.drawPixel(x, y, colorFor(x, y, event.acceleration.x, event.acceleration.y));
    }
  }

  int centerX = MATRIX_WIDTH / 2 + (int)(event.acceleration.x * 2.2f);
  int centerY = MATRIX_HEIGHT / 2 + (int)(event.acceleration.y * 1.4f);
  matrix.fillCircle(centerX, centerY, 4, matrix.color565(255, 255, 255));

  matrix.show();
  delay(FRAME_DELAY_MS);
}
