#include "SignDisplay.h"

#define CELL 4
#define SNAKE_COLS (SIGN_WIDTH / CELL)
#define SNAKE_ROWS (SIGN_HEIGHT / CELL)
#define MAX_SNAKE 96

int snakeX[MAX_SNAKE];
int snakeY[MAX_SNAKE];
int snakeLength = 10;
int foodX = 20;
int foodY = 4;
int dx = 1;
int dy = 0;

bool occupied(int x, int y) {
  for (int i = 0; i < snakeLength; i++) {
    if (snakeX[i] == x && snakeY[i] == y) return true;
  }
  return false;
}

void placeFood() {
  for (int attempt = 0; attempt < 100; attempt++) {
    foodX = random(SNAKE_COLS);
    foodY = random(SNAKE_ROWS);
    if (!occupied(foodX, foodY)) return;
  }
}

void resetSnake() {
  snakeLength = 10;
  for (int i = 0; i < snakeLength; i++) {
    snakeX[i] = 8 - i;
    snakeY[i] = SNAKE_ROWS / 2;
  }
  dx = 1;
  dy = 0;
  placeFood();
}

void setup() {
  signBegin();
  randomSeed(analogRead(A0) + millis());
  resetSnake();
}

void loop() {
  int headX = snakeX[0];
  int headY = snakeY[0];
  int options[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
  int best = 0;
  int bestDistance = 1000;
  for (int i = 0; i < 4; i++) {
    if (options[i][0] == -dx && options[i][1] == -dy) continue;
    int nx = (headX + options[i][0] + SNAKE_COLS) % SNAKE_COLS;
    int ny = (headY + options[i][1] + SNAKE_ROWS) % SNAKE_ROWS;
    int d = abs(nx - foodX) + abs(ny - foodY);
    if (!occupied(nx, ny) && d < bestDistance) {
      bestDistance = d;
      best = i;
    }
  }
  dx = options[best][0];
  dy = options[best][1];

  int nextX = (headX + dx + SNAKE_COLS) % SNAKE_COLS;
  int nextY = (headY + dy + SNAKE_ROWS) % SNAKE_ROWS;
  if (occupied(nextX, nextY)) {
    resetSnake();
  } else {
    for (int i = snakeLength - 1; i > 0; i--) {
      snakeX[i] = snakeX[i - 1];
      snakeY[i] = snakeY[i - 1];
    }
    snakeX[0] = nextX;
    snakeY[0] = nextY;
    if (nextX == foodX && nextY == foodY) {
      if (snakeLength < MAX_SNAKE) snakeLength++;
      placeFood();
    }
  }

  matrix.fillScreen(0);
  matrix.fillRect(foodX * CELL + 1, foodY * CELL + 1, CELL - 1, CELL - 1, signRgb(255, 60, 80));
  for (int i = 0; i < snakeLength; i++) {
    uint16_t color = i == 0 ? signRgb(255, 240, 90) : signWheel((uint8_t)(80 + i * 4));
    matrix.fillRect(snakeX[i] * CELL, snakeY[i] * CELL, CELL - 1, CELL - 1, color);
  }
  matrix.show();
  delay(90);
}
