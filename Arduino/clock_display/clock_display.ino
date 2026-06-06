#include "SignDisplay.h"

const char *days[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};

void setup() {
  signBegin();
}

void loop() {
  unsigned long seconds = millis() / 1000UL;
  int hour = (12 + (seconds / 3600UL)) % 24;
  int minute = (seconds / 60UL) % 60;
  int second = seconds % 60;
  int day = (seconds / 86400UL) % 7;

  char timeText[8];
  snprintf(timeText, sizeof(timeText), "%02d:%02d", hour, minute);
  char dateText[20];
  snprintf(dateText, sizeof(dateText), "%s  %02d SEC", days[day], second);

  matrix.fillScreen(0);
  uint16_t timeColor = signRgb(120, 235, 255);
  uint16_t dimColor = signRgb(25, 70, 100);
  uint16_t dateColor = signRgb(255, 210, 90);
  signDrawCenteredText(5, timeText, timeColor, 2);
  if (second % 2 == 0) {
    int colonX = (SIGN_WIDTH - signTextWidth(timeText, 2)) / 2 + 2 * 6 * 2 + 5;
    matrix.fillRect(colonX, 9, 2, 2, dimColor);
    matrix.fillRect(colonX, 17, 2, 2, dimColor);
  }
  signDrawCenteredText(24, dateText, dateColor, 1);
  matrix.show();
  delay(120);
}
