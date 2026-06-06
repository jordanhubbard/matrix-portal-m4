#include "SignDisplay.h"

float ballX = SIGN_WIDTH / 2.0f;
float ballY = SIGN_HEIGHT / 2.0f;
float velX = 1.15f;
float velY = 0.72f;
int leftPaddle = 10;
int rightPaddle = 18;
int leftScore = 0;
int rightScore = 0;

void setup() {
  signBegin();
}

void loop() {
  if ((int)ballY < leftPaddle + 4) leftPaddle--;
  if ((int)ballY > leftPaddle + 4) leftPaddle++;
  if ((int)ballY < rightPaddle + 4) rightPaddle--;
  if ((int)ballY > rightPaddle + 4) rightPaddle++;
  if (leftPaddle < 1) leftPaddle = 1;
  if (leftPaddle > SIGN_HEIGHT - 9) leftPaddle = SIGN_HEIGHT - 9;
  if (rightPaddle < 1) rightPaddle = 1;
  if (rightPaddle > SIGN_HEIGHT - 9) rightPaddle = SIGN_HEIGHT - 9;

  ballX += velX;
  ballY += velY;
  if (ballY <= 1 || ballY >= SIGN_HEIGHT - 2) {
    velY = -velY;
  }
  if (ballX <= 4 && ballY >= leftPaddle && ballY <= leftPaddle + 8) {
    velX = fabsf(velX);
    velY += ((ballY - leftPaddle) - 4) * 0.08f;
  }
  if (ballX >= SIGN_WIDTH - 5 && ballY >= rightPaddle && ballY <= rightPaddle + 8) {
    velX = -fabsf(velX);
    velY += ((ballY - rightPaddle) - 4) * 0.08f;
  }
  if (ballX < 0 || ballX >= SIGN_WIDTH) {
    if (ballX < 0) rightScore++; else leftScore++;
    ballX = SIGN_WIDTH / 2.0f;
    ballY = SIGN_HEIGHT / 2.0f;
    velX = ballX < 1 ? 1.15f : -1.15f;
    velY = ((millis() / 250) % 2 == 0) ? 0.7f : -0.7f;
  }

  matrix.fillScreen(0);
  for (int y = 2; y < SIGN_HEIGHT - 2; y += 4) {
    matrix.drawPixel(SIGN_WIDTH / 2, y, signRgb(50, 70, 90));
  }
  matrix.fillRect(1, leftPaddle, 2, 8, signRgb(80, 220, 255));
  matrix.fillRect(SIGN_WIDTH - 3, rightPaddle, 2, 8, signRgb(255, 120, 160));
  matrix.fillCircle((int)ballX, (int)ballY, 2, signRgb(255, 255, 255));
  char score[8];
  snprintf(score, sizeof(score), "%d-%d", leftScore % 10, rightScore % 10);
  signDrawCenteredText(2, score, signRgb(255, 220, 90), 1);
  matrix.show();
  delay(22);
}
