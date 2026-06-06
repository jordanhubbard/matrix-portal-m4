#include <Adafruit_Protomatter.h>

uint8_t rgbPins[]  = {7, 8, 9, 10, 11, 12};
uint8_t addrPins[] = {17, 18, 19, 20, 21};
uint8_t clockPin   = 14;
uint8_t latchPin   = 15;
uint8_t oePin      = 16;

#ifndef PANEL_COUNT
#define PANEL_COUNT 4
#endif

#ifndef PANEL_WIDTH
#define PANEL_WIDTH 32
#endif

#ifndef PANEL_HEIGHT
#define PANEL_HEIGHT 32
#endif
#define MATRIX_WIDTH (PANEL_COUNT * PANEL_WIDTH)
#define MATRIX_HEIGHT PANEL_HEIGHT
#define TILE_SIZE 4
#define GRID_WIDTH (MATRIX_WIDTH / TILE_SIZE)
#define GRID_HEIGHT (MATRIX_HEIGHT / TILE_SIZE)
#define MAX_TILES (GRID_WIDTH * GRID_HEIGHT)
#define GHOST_COUNT 4
#define FRAME_DELAY_MS 28

#if PANEL_COUNT < 1 || PANEL_COUNT > 4
#error "PANEL_COUNT should be 1, 2, 3, or 4 for this demo."
#endif

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

struct Actor {
  int x;
  int y;
  int dx;
  int dy;
  uint16_t color;
};

Actor pacman;
Actor ghosts[GHOST_COUNT];
bool pellets[GRID_HEIGHT][GRID_WIDTH];
bool powerPellets[GRID_HEIGHT][GRID_WIDTH];
uint16_t wallColor;
uint16_t wallHighlightColor;
uint16_t pelletColor;
uint16_t powerColor;
uint16_t pacmanColor;
uint16_t frightenedColor;
uint16_t blackColor;
uint16_t ghostColors[GHOST_COUNT];
int pelletsLeft = 0;
int score = 0;
int powerTimer = 0;
unsigned long frameNumber = 0;

const int dirX[4] = {1, 0, -1, 0};
const int dirY[4] = {0, 1, 0, -1};

int iabs(int value) {
  return value < 0 ? -value : value;
}

int wrappedTileX(int x) {
  if (x < 0) return GRID_WIDTH - 1;
  if (x >= GRID_WIDTH) return 0;
  return x;
}

bool isWallTile(int x, int y) {
  x = wrappedTileX(x);
  if (y < 0 || y >= GRID_HEIGHT) {
    return true;
  }
  if ((x == 0 || x == GRID_WIDTH - 1) && y == GRID_HEIGHT / 2) {
    return false;
  }
  if (x == 0 || x == GRID_WIDTH - 1 || y == 0 || y == GRID_HEIGHT - 1) {
    return true;
  }

  int localX = x % 8;
  if ((localX == 3 || localX == 4) && (y == 2 || y == GRID_HEIGHT - 3)) {
    return true;
  }
  if (GRID_WIDTH >= 16 && (x % 10 == 0) && (y == 1 || y == GRID_HEIGHT - 2)) {
    return true;
  }
  return false;
}

int tileForPixel(int pixel) {
  if (pixel < 0) return GRID_WIDTH - 1;
  if (pixel >= MATRIX_WIDTH) return 0;
  return pixel / TILE_SIZE;
}

int tileCenter(int tile) {
  return tile * TILE_SIZE + TILE_SIZE / 2;
}

bool atTileCenter(const Actor &actor) {
  return (actor.x % TILE_SIZE == TILE_SIZE / 2) &&
         (actor.y % TILE_SIZE == TILE_SIZE / 2);
}

bool canMoveTo(int x, int y) {
  if (x < 0) x = MATRIX_WIDTH - 1;
  if (x >= MATRIX_WIDTH) x = 0;
  if (y < 0 || y >= MATRIX_HEIGHT) {
    return false;
  }
  return !isWallTile(tileForPixel(x), y / TILE_SIZE);
}

void moveActor(Actor &actor) {
  int nextX = actor.x + actor.dx;
  int nextY = actor.y + actor.dy;
  if (nextX < 0) nextX = MATRIX_WIDTH - 1;
  if (nextX >= MATRIX_WIDTH) nextX = 0;
  if (canMoveTo(nextX, nextY)) {
    actor.x = nextX;
    actor.y = nextY;
  }
}

void placeActor(Actor &actor, int tileX, int tileY, int dx, int dy) {
  tileX = wrappedTileX(tileX);
  if (isWallTile(tileX, tileY)) {
    bool found = false;
    for (int radius = 1; radius < GRID_WIDTH; radius++) {
      for (int oy = -radius; oy <= radius; oy++) {
        for (int ox = -radius; ox <= radius; ox++) {
          int candidateX = wrappedTileX(tileX + ox);
          int candidateY = tileY + oy;
          if (!isWallTile(candidateX, candidateY)) {
            tileX = candidateX;
            tileY = candidateY;
            found = true;
            break;
          }
        }
        if (found) break;
      }
      if (found) break;
    }
  }
  actor.x = tileCenter(tileX);
  actor.y = tileCenter(tileY);
  actor.dx = dx;
  actor.dy = dy;
}

void resetPellets() {
  pelletsLeft = 0;
  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      bool open = !isWallTile(x, y);
      bool ghostStart = x >= GRID_WIDTH / 2 - 2 && x <= GRID_WIDTH / 2 + 2 &&
                        y >= GRID_HEIGHT / 2 - 1 && y <= GRID_HEIGHT / 2 + 1;
      pellets[y][x] = open && !ghostStart;
      powerPellets[y][x] = false;
      if (pellets[y][x]) {
        pelletsLeft++;
      }
    }
  }

  int corners[4][2] = {
    {1, 1},
    {GRID_WIDTH - 2, 1},
    {1, GRID_HEIGHT - 2},
    {GRID_WIDTH - 2, GRID_HEIGHT - 2}
  };
  for (int i = 0; i < 4; i++) {
    int x = corners[i][0];
    int y = corners[i][1];
    if (!isWallTile(x, y)) {
      powerPellets[y][x] = true;
    }
  }
}

void resetRound() {
  resetPellets();
  score = 0;
  powerTimer = 0;
  placeActor(pacman, 1, GRID_HEIGHT / 2, 1, 0);

  for (int i = 0; i < GHOST_COUNT; i++) {
    ghosts[i].color = ghostColors[i];
    placeActor(ghosts[i], GRID_WIDTH / 2 - 2 + i, GRID_HEIGHT / 2, i % 2 == 0 ? 1 : -1, 0);
  }
}

int chooseFallbackDirection(const Actor &actor) {
  int current = 0;
  for (int i = 0; i < 4; i++) {
    if (dirX[i] == actor.dx && dirY[i] == actor.dy) {
      current = i;
      break;
    }
  }
  for (int attempt = 0; attempt < 4; attempt++) {
    int d = (current + attempt) % 4;
    int nextX = actor.x + dirX[d];
    int nextY = actor.y + dirY[d];
    if (canMoveTo(nextX, nextY)) {
      return d;
    }
  }
  return current;
}

int choosePacmanDirection() {
  bool seen[GRID_HEIGHT][GRID_WIDTH];
  int firstDir[GRID_HEIGHT][GRID_WIDTH];
  int qx[MAX_TILES];
  int qy[MAX_TILES];

  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      seen[y][x] = false;
      firstDir[y][x] = -1;
    }
  }

  int startX = tileForPixel(pacman.x);
  int startY = pacman.y / TILE_SIZE;
  int head = 0;
  int tail = 0;
  qx[tail] = startX;
  qy[tail] = startY;
  tail++;
  seen[startY][startX] = true;

  while (head < tail) {
    int x = qx[head];
    int y = qy[head];
    head++;

    if ((pellets[y][x] || powerPellets[y][x]) && !(x == startX && y == startY)) {
      return firstDir[y][x];
    }

    for (int d = 0; d < 4; d++) {
      int nx = wrappedTileX(x + dirX[d]);
      int ny = y + dirY[d];
      if (ny < 0 || ny >= GRID_HEIGHT || seen[ny][nx] || isWallTile(nx, ny)) {
        continue;
      }
      seen[ny][nx] = true;
      firstDir[ny][nx] = firstDir[y][x] < 0 ? d : firstDir[y][x];
      qx[tail] = nx;
      qy[tail] = ny;
      tail++;
    }
  }

  return chooseFallbackDirection(pacman);
}

int chooseGhostDirection(const Actor &ghost, int ghostIndex) {
  int bestDir = chooseFallbackDirection(ghost);
  int bestScore = powerTimer > 0 ? -32000 : 32000;
  int targetTileX = tileForPixel(pacman.x);
  int targetTileY = pacman.y / TILE_SIZE;

  if ((frameNumber / 180 + ghostIndex) % 5 == 0) {
    targetTileX = ghostIndex % 2 == 0 ? 1 : GRID_WIDTH - 2;
    targetTileY = ghostIndex < 2 ? 1 : GRID_HEIGHT - 2;
  }

  int reverseX = -ghost.dx;
  int reverseY = -ghost.dy;
  for (int d = 0; d < 4; d++) {
    if (dirX[d] == reverseX && dirY[d] == reverseY) {
      bool hasOtherMove = false;
      for (int check = 0; check < 4; check++) {
        if (check == d) continue;
        if (canMoveTo(ghost.x + dirX[check], ghost.y + dirY[check])) {
          hasOtherMove = true;
        }
      }
      if (hasOtherMove) {
        continue;
      }
    }

    int nextX = ghost.x + dirX[d];
    int nextY = ghost.y + dirY[d];
    if (!canMoveTo(nextX, nextY)) {
      continue;
    }
    int tileX = tileForPixel(nextX);
    int tileY = nextY / TILE_SIZE;
    int distance = iabs(tileX - targetTileX) + iabs(tileY - targetTileY);
    if (powerTimer > 0) {
      if (distance > bestScore) {
        bestScore = distance;
        bestDir = d;
      }
    } else if (distance < bestScore) {
      bestScore = distance;
      bestDir = d;
    }
  }
  return bestDir;
}

void consumePellet() {
  int tx = tileForPixel(pacman.x);
  int ty = pacman.y / TILE_SIZE;
  if (powerPellets[ty][tx]) {
    powerPellets[ty][tx] = false;
    powerTimer = 180;
    score += 50;
  }
  if (pellets[ty][tx]) {
    pellets[ty][tx] = false;
    pelletsLeft--;
    score += 10;
  }
  if (pelletsLeft <= 0) {
    delay(300);
    resetRound();
  }
}

void handleCollisions() {
  for (int i = 0; i < GHOST_COUNT; i++) {
    int dx = pacman.x - ghosts[i].x;
    int dy = pacman.y - ghosts[i].y;
    if (dx > MATRIX_WIDTH / 2) dx -= MATRIX_WIDTH;
    if (dx < -MATRIX_WIDTH / 2) dx += MATRIX_WIDTH;
    if (iabs(dx) + iabs(dy) <= 4) {
      if (powerTimer > 0) {
        score += 200;
        placeActor(ghosts[i], GRID_WIDTH / 2 - 2 + i, GRID_HEIGHT / 2, i % 2 == 0 ? 1 : -1, 0);
      } else {
        matrix.fillScreen(matrix.color565(80, 0, 0));
        matrix.show();
        delay(250);
        resetRound();
      }
    }
  }
}

void drawMaze() {
  matrix.fillScreen(blackColor);
  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      int px = x * TILE_SIZE;
      int py = y * TILE_SIZE;
      if (isWallTile(x, y)) {
        matrix.fillRect(px, py, TILE_SIZE, TILE_SIZE, wallColor);
        matrix.drawPixel(px + 1, py + 1, wallHighlightColor);
      } else if (powerPellets[y][x]) {
        matrix.fillRect(px + 1, py + 1, 2, 2, powerColor);
      } else if (pellets[y][x]) {
        matrix.drawPixel(px + TILE_SIZE / 2, py + TILE_SIZE / 2, pelletColor);
      }
    }
  }
}

void drawPacman() {
  matrix.fillCircle(pacman.x, pacman.y, 2, pacmanColor);
  if ((frameNumber / 5) % 2 == 0) {
    if (pacman.dx > 0) {
      matrix.fillRect(pacman.x + 1, pacman.y - 1, 3, 3, blackColor);
    } else if (pacman.dx < 0) {
      matrix.fillRect(pacman.x - 3, pacman.y - 1, 3, 3, blackColor);
    } else if (pacman.dy > 0) {
      matrix.fillRect(pacman.x - 1, pacman.y + 1, 3, 3, blackColor);
    } else {
      matrix.fillRect(pacman.x - 1, pacman.y - 3, 3, 3, blackColor);
    }
  }
}

void drawGhost(const Actor &ghost, uint16_t color) {
  matrix.fillCircle(ghost.x, ghost.y - 1, 2, color);
  matrix.fillRect(ghost.x - 2, ghost.y - 1, 5, 4, color);
  matrix.drawPixel(ghost.x - 1, ghost.y, matrix.color565(255, 255, 255));
  matrix.drawPixel(ghost.x + 1, ghost.y, matrix.color565(255, 255, 255));
  matrix.drawPixel(ghost.x - 1 + ghost.dx, ghost.y, blackColor);
  matrix.drawPixel(ghost.x + 1 + ghost.dx, ghost.y, blackColor);
}

void drawScoreTicks() {
  int filled = score / 200;
  for (int i = 0; i < filled && i < MATRIX_WIDTH / 4; i++) {
    matrix.drawPixel(2 + i * 4, MATRIX_HEIGHT - 1, matrix.color565(255, 160, 0));
  }
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0) + millis());

  ProtomatterStatus status = matrix.begin();
  Serial.print("Protomatter begin() status: ");
  Serial.println((int)status);
  if (status != PROTOMATTER_OK) {
    return;
  }

  wallColor = matrix.color565(0, 24, 180);
  wallHighlightColor = matrix.color565(40, 90, 255);
  pelletColor = matrix.color565(255, 214, 150);
  powerColor = matrix.color565(255, 255, 255);
  pacmanColor = matrix.color565(255, 220, 0);
  frightenedColor = matrix.color565(40, 120, 255);
  blackColor = matrix.color565(0, 0, 0);
  ghostColors[0] = matrix.color565(255, 0, 0);
  ghostColors[1] = matrix.color565(255, 140, 220);
  ghostColors[2] = matrix.color565(0, 220, 220);
  ghostColors[3] = matrix.color565(255, 140, 0);

  resetRound();
}

void loop() {
  if (atTileCenter(pacman)) {
    int d = choosePacmanDirection();
    pacman.dx = dirX[d];
    pacman.dy = dirY[d];
  }
  moveActor(pacman);
  consumePellet();

  for (int i = 0; i < GHOST_COUNT; i++) {
    if (atTileCenter(ghosts[i])) {
      int d = chooseGhostDirection(ghosts[i], i);
      ghosts[i].dx = dirX[d];
      ghosts[i].dy = dirY[d];
    }
    if ((frameNumber + i) % 2 == 0) {
      moveActor(ghosts[i]);
    }
  }

  if (powerTimer > 0) {
    powerTimer--;
  }
  handleCollisions();

  drawMaze();
  drawPacman();
  for (int i = 0; i < GHOST_COUNT; i++) {
    uint16_t color = powerTimer > 0 && (powerTimer > 45 || (frameNumber / 6) % 2 == 0)
      ? frightenedColor
      : ghosts[i].color;
    drawGhost(ghosts[i], color);
  }
  drawScoreTicks();
  matrix.show();

  frameNumber++;
  delay(FRAME_DELAY_MS);
}
