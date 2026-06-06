#include "SignDisplay.h"

const char *messages[] = {
  "MATRIX PORTAL M4",
  "32X32 RGB SIGN",
  "ARDUINO CLI + SDL IDE",
  "CHAIN 2 TO 4 PANELS"
};

int messageIndex = 0;
int scrollX = SIGN_WIDTH;
uint8_t hue = 0;

void setup() {
  signBegin();
}

void loop() {
  matrix.fillScreen(0);

  for (int x = 0; x < SIGN_WIDTH; x += 8) {
    uint16_t color = signWheel((uint8_t)(hue + x * 2));
    matrix.drawPixel((x + hue / 4) % SIGN_WIDTH, 2, color);
    matrix.drawPixel((SIGN_WIDTH - 1 - x + hue / 3) % SIGN_WIDTH, SIGN_HEIGHT - 3, color);
  }

  const char *message = messages[messageIndex];
  uint16_t textColor = signWheel(hue);
  signDrawText(scrollX, 11, message, textColor, 1);
  matrix.drawLine(0, 0, SIGN_WIDTH - 1, 0, signWheel(hue + 80));
  matrix.drawLine(0, SIGN_HEIGHT - 1, SIGN_WIDTH - 1, SIGN_HEIGHT - 1, signWheel(hue + 160));
  matrix.show();

  scrollX--;
  if (scrollX < -signTextWidth(message)) {
    messageIndex = (messageIndex + 1) % (sizeof(messages) / sizeof(messages[0]));
    scrollX = SIGN_WIDTH + 8;
  }
  hue += 2;
  delay(28);
}
