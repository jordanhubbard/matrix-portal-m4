#include "SignDisplay.h"

#define BRICK_COLS 16
#define BRICK_ROWS 4

bool bricks[BRICK_ROWS][BRICK_COLS];
float ballX = SIGN_WIDTH / 2.0f;
float ballY = SIGN_HEIGHT - 9.0f;
float velX = 1.2f;
float velY = -0.9f;
int paddleX = SIGN_WIDTH / 2 - 8;

void resetBricks() {
  for (int y = 0; y < BRICK_ROWS; y++) {
    for (int x = 0; x < BRICK_COLS; x++) {
      bricks[y][x] = true;
    }
  }
}

void setup() {
  signBegin();
  resetBricks();
}

void loop() {
  paddleX += (int)ballX > paddleX + 8 ? 2 : -2;
  if (paddleX < 0) paddleX = 0;
  if (paddleX > SIGN_WIDTH - 16) paddleX = SIGN_WIDTH - 16;

  ballX += velX;
  ballY += velY;
  if (ballX <= 1 || ballX >= SIGN_WIDTH - 2) velX = -velX;
  if (ballY <= 1) velY = fabsf(velY);
  if (ballY >= SIGN_HEIGHT - 6 && ballX >= paddleX && ballX <= paddleX + 16) {
    velY = -fabsf(velY);
    velX += ((ballX - paddleX) - 8) * 0.04f;
  }
  if (ballY >= SIGN_HEIGHT) {
    ballX = SIGN_WIDTH / 2.0f;
    ballY = SIGN_HEIGHT - 9.0f;
    velX = 1.2f;
    velY = -0.9f;
  }

  int brickW = SIGN_WIDTH / BRICK_COLS;
  int bx = (int)ballX / brickW;
  int by = ((int)ballY - 3) / 4;
  if (by >= 0 && by < BRICK_ROWS && bx >= 0 && bx < BRICK_COLS && bricks[by][bx]) {
    bricks[by][bx] = false;
    velY = -velY;
  }

  bool any = false;
  matrix.fillScreen(0);
  for (int y = 0; y < BRICK_ROWS; y++) {
    for (int x = 0; x < BRICK_COLS; x++) {
      if (bricks[y][x]) {
        any = true;
        matrix.fillRect(x * brickW, 3 + y * 4, brickW - 1, 3, signWheel((uint8_t)(y * 50 + x * 5)));
      }
    }
  }
  if (!any) resetBricks();
  matrix.fillRect(paddleX, SIGN_HEIGHT - 3, 16, 2, signRgb(90, 220, 255));
  matrix.fillCircle((int)ballX, (int)ballY, 2, signRgb(255, 255, 255));
  matrix.show();
  delay(24);
}
