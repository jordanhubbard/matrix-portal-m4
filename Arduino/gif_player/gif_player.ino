#include "SignDisplay.h"

void drawFrameSprite(int cx, int cy, int frame) {
  uint16_t body = signWheel((uint8_t)(frame * 20));
  uint16_t eye = signRgb(255, 255, 255);
  uint16_t dark = signRgb(0, 0, 0);
  int radius = 5 + (frame % 3);
  matrix.fillCircle(cx, cy, radius, body);
  matrix.fillCircle(cx - 2, cy - 1, 1, eye);
  matrix.fillCircle(cx + 2, cy - 1, 1, eye);
  matrix.drawPixel(cx - 2, cy - 1, dark);
  matrix.drawPixel(cx + 2, cy - 1, dark);
  if (frame % 2 == 0) {
    matrix.drawLine(cx - 3, cy + 3, cx + 3, cy + 3, eye);
  } else {
    matrix.drawLine(cx - 3, cy + 2, cx, cy + 4, eye);
    matrix.drawLine(cx, cy + 4, cx + 3, cy + 2, eye);
  }
}

void setup() {
  signBegin();
}

void loop() {
  unsigned long frame = millis() / 90UL;
  int spriteX = (int)(frame % (SIGN_WIDTH + 24)) - 12;
  int spriteY = 16 + (int)(sinf(frame * 0.22f) * 7.0f);

  matrix.fillScreen(signRgb(2, 4, 10));
  signDrawText(2, 2, "GIF", signRgb(255, 220, 80), 1);
  signDrawText(2, 22, "FRAME", signRgb(90, 150, 255), 1);
  for (int i = 0; i < 8; i++) {
    int x = 42 + i * 10;
    matrix.drawRect(x, 3, 8, 8, signRgb(30, 60, 100));
    if ((frame / 2) % 8 == (unsigned long)i) {
      matrix.fillRect(x + 2, 5, 4, 4, signRgb(255, 220, 120));
    }
  }
  drawFrameSprite(spriteX, spriteY, frame % 12);
  matrix.show();
  delay(35);
}
