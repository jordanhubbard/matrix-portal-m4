#include "SignDisplay.h"

#define BAR_COUNT 32

int peaks[BAR_COUNT];

void setup() {
  signBegin();
  for (int i = 0; i < BAR_COUNT; i++) peaks[i] = 0;
}

void loop() {
  unsigned long t = millis();
  matrix.fillScreen(0);
  int barWidth = SIGN_WIDTH / BAR_COUNT;
  for (int i = 0; i < BAR_COUNT; i++) {
    float wave = sinf(t * 0.004f + i * 0.45f) + sinf(t * 0.009f + i * 0.19f);
    int level = 3 + (int)((wave + 2.0f) * 7.0f);
    level += random(4);
    if (level > SIGN_HEIGHT - 2) level = SIGN_HEIGHT - 2;
    if (level > peaks[i]) peaks[i] = level;
    else if (peaks[i] > 0) peaks[i]--;

    uint16_t color = level > 24 ? signRgb(255, 45, 45) :
                     level > 16 ? signRgb(255, 210, 45) :
                                  signRgb(60, 235, 130);
    matrix.fillRect(i * barWidth, SIGN_HEIGHT - level, barWidth - 1, level, color);
    matrix.drawPixel(i * barWidth, SIGN_HEIGHT - 1 - peaks[i], signRgb(255, 255, 255));
  }
  signDrawText(2, 2, "VU", signRgb(120, 200, 255), 1);
  matrix.show();
  delay(35);
}
