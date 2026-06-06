#include "SignDisplay.h"

const char *conditions[] = {"SUN", "CLOUD", "RAIN", "WIND"};

void drawSun(int cx, int cy, uint16_t color) {
  matrix.fillCircle(cx, cy, 5, color);
  for (int i = 0; i < 8; i++) {
    float a = i * TWO_PI / 8.0f;
    matrix.drawLine(cx + (int)(cosf(a) * 7), cy + (int)(sinf(a) * 7),
                    cx + (int)(cosf(a) * 10), cy + (int)(sinf(a) * 10), color);
  }
}

void drawCloud(int x, int y, uint16_t color) {
  matrix.fillCircle(x + 6, y + 6, 5, color);
  matrix.fillCircle(x + 13, y + 4, 6, color);
  matrix.fillCircle(x + 20, y + 7, 5, color);
  matrix.fillRect(x + 4, y + 7, 22, 6, color);
}

void setup() {
  signBegin();
}

void loop() {
  unsigned long t = millis();
  int mode = (t / 5000UL) % 4;
  int temp = 72 + (int)(sinf(t * 0.0012f) * 9.0f);
  int humid = 45 + (int)(cosf(t * 0.0008f) * 20.0f);
  int wind = 5 + (int)(sinf(t * 0.0017f + 2.0f) * 4.0f);

  char tempText[12];
  snprintf(tempText, sizeof(tempText), "%dF", temp);
  char detailText[18];
  snprintf(detailText, sizeof(detailText), "%s H%d W%d", conditions[mode], humid, wind);

  matrix.fillScreen(0);
  if (mode == 0) {
    drawSun(15, 15, signRgb(255, 210, 0));
  } else if (mode == 1) {
    drawCloud(2, 7, signRgb(150, 180, 210));
  } else if (mode == 2) {
    drawCloud(2, 5, signRgb(120, 145, 180));
    for (int i = 0; i < 5; i++) {
      matrix.drawLine(7 + i * 5, 21, 5 + i * 5, 27, signRgb(50, 150, 255));
    }
  } else {
    for (int i = 0; i < 5; i++) {
      int y = 8 + i * 4;
      matrix.drawLine(3, y, 26 + (int)(sinf(t * 0.004f + i) * 4), y, signRgb(120, 220, 255));
    }
  }

  signDrawText(36, 4, tempText, signRgb(255, 235, 120), 2);
  signDrawText(36, 23, detailText, signRgb(170, 230, 255), 1);
  matrix.show();
  delay(80);
}
