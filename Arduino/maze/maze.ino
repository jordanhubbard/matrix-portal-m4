#include "SignDisplay.h"

#define MATRIX_WIDTH SIGN_WIDTH
#define MATRIX_HEIGHT SIGN_HEIGHT
#define CELL_COLS ((MATRIX_WIDTH - 1) / 2)
#define CELL_ROWS ((MATRIX_HEIGHT - 1) / 2)
#define MAX_CELLS (CELL_COLS * CELL_ROWS)
#define STEP_DELAY_MS 10

struct Cell {
  uint8_t x;
  uint8_t y;
};

bool path[MATRIX_HEIGHT][MATRIX_WIDTH];
bool visited[CELL_ROWS][CELL_COLS];
bool solvingVisited[CELL_ROWS][CELL_COLS];
bool solution[MATRIX_HEIGHT][MATRIX_WIDTH];
Cell stackCells[MAX_CELLS];
int stackTop = 0;

int dx[] = {0, 0, 1, -1};
int dy[] = {-1, 1, 0, 0};

uint16_t wallColor() {
  return matrix.color565(0, 0, 0);
}

uint16_t pathColor() {
  return matrix.color565(3, 22, 18);
}

uint16_t routeColor() {
  return matrix.color565(255, 176, 42);
}

uint16_t runnerColor() {
  return matrix.color565(255, 24, 24);
}

uint16_t exitColor() {
  return matrix.color565(28, 190, 255);
}

int pixelX(int cellX) {
  return cellX * 2 + 1;
}

int pixelY(int cellY) {
  return cellY * 2 + 1;
}

void clearState() {
  memset(path, 0, sizeof(path));
  memset(visited, 0, sizeof(visited));
  memset(solvingVisited, 0, sizeof(solvingVisited));
  memset(solution, 0, sizeof(solution));
  stackTop = 0;
}

void shuffleDirections(int directions[4]) {
  for (int i = 0; i < 4; i++) {
    int r = i + random(4 - i);
    int temp = directions[i];
    directions[i] = directions[r];
    directions[r] = temp;
  }
}

void carveBetween(int x1, int y1, int x2, int y2) {
  int px1 = pixelX(x1);
  int py1 = pixelY(y1);
  int px2 = pixelX(x2);
  int py2 = pixelY(y2);

  path[py1][px1] = true;
  path[(py1 + py2) / 2][(px1 + px2) / 2] = true;
  path[py2][px2] = true;
}

void generateMaze() {
  clearState();

  stackCells[stackTop++] = {0, 0};
  visited[0][0] = true;
  path[pixelY(0)][pixelX(0)] = true;

  while (stackTop > 0) {
    Cell current = stackCells[stackTop - 1];
    int directions[] = {0, 1, 2, 3};
    shuffleDirections(directions);

    bool moved = false;
    for (int i = 0; i < 4; i++) {
      int nx = current.x + dx[directions[i]];
      int ny = current.y + dy[directions[i]];

      if (nx >= 0 && ny >= 0 && nx < CELL_COLS && ny < CELL_ROWS && !visited[ny][nx]) {
        carveBetween(current.x, current.y, nx, ny);
        visited[ny][nx] = true;
        stackCells[stackTop++] = {(uint8_t)nx, (uint8_t)ny};
        moved = true;
        break;
      }
    }

    if (!moved) {
      stackTop--;
    }
  }

  path[0][1] = true;
  path[MATRIX_HEIGHT - 1][MATRIX_WIDTH - 2] = true;
}

void drawMaze(int runnerX = -1, int runnerY = -1) {
  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      uint16_t color = path[y][x] ? pathColor() : wallColor();
      if (solution[y][x]) {
        color = routeColor();
      }
      matrix.drawPixel(x, y, color);
    }
  }

  matrix.drawPixel(1, 0, exitColor());
  matrix.drawPixel(MATRIX_WIDTH - 2, MATRIX_HEIGHT - 1, exitColor());

  if (runnerX >= 0 && runnerY >= 0) {
    int px = pixelX(runnerX);
    int py = pixelY(runnerY);
    matrix.drawPixel(px, py, runnerColor());
    matrix.drawPixel(px - 1, py, runnerColor());
    matrix.drawPixel(px + 1, py, runnerColor());
    matrix.drawPixel(px, py - 1, runnerColor());
    matrix.drawPixel(px, py + 1, runnerColor());
  }

  matrix.show();
}

bool canMove(int x, int y, int direction) {
  int nx = x + dx[direction];
  int ny = y + dy[direction];
  if (nx < 0 || ny < 0 || nx >= CELL_COLS || ny >= CELL_ROWS || solvingVisited[ny][nx]) {
    return false;
  }

  int wallX = (pixelX(x) + pixelX(nx)) / 2;
  int wallY = (pixelY(y) + pixelY(ny)) / 2;
  return path[wallY][wallX];
}

bool solveMaze(int x, int y) {
  solvingVisited[y][x] = true;
  solution[pixelY(y)][pixelX(x)] = true;
  drawMaze(x, y);
  delay(STEP_DELAY_MS);

  if (x == CELL_COLS - 1 && y == CELL_ROWS - 1) {
    return true;
  }

  int directions[] = {0, 1, 2, 3};
  shuffleDirections(directions);
  for (int i = 0; i < 4; i++) {
    int direction = directions[i];
    if (canMove(x, y, direction)) {
      int nx = x + dx[direction];
      int ny = y + dy[direction];
      int wallX = (pixelX(x) + pixelX(nx)) / 2;
      int wallY = (pixelY(y) + pixelY(ny)) / 2;
      solution[wallY][wallX] = true;

      if (solveMaze(nx, ny)) {
        return true;
      }

      solution[wallY][wallX] = false;
    }
  }

  solution[pixelY(y)][pixelX(x)] = false;
  drawMaze(x, y);
  delay(STEP_DELAY_MS);
  return false;
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

  generateMaze();
  drawMaze();
  delay(600);
  solveMaze(0, 0);
}

void loop() {
}
