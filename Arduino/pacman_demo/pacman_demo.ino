#include "SignDisplay.h"

#define MATRIX_WIDTH SIGN_WIDTH
#define MATRIX_HEIGHT SIGN_HEIGHT
#define TILE_SIZE 4
#define GRID_WIDTH (MATRIX_WIDTH / TILE_SIZE)
#define GRID_HEIGHT (MATRIX_HEIGHT / TILE_SIZE)
#define MAX_TILES (GRID_WIDTH * GRID_HEIGHT)
#define GHOST_COUNT 4
#define FRAME_DELAY_MS 28

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

bool isReverseDirection(const Actor &actor, int d) {
  return dirX[d] == -actor.dx && dirY[d] == -actor.dy;
}

bool canEnterTile(const Actor &actor, int d) {
  int tileX = wrappedTileX(tileForPixel(actor.x) + dirX[d]);
  int tileY = actor.y / TILE_SIZE + dirY[d];
  return tileY >= 0 && tileY < GRID_HEIGHT && !isWallTile(tileX, tileY);
}

int collectLegalDirections(const Actor &actor, int dirs[4], bool allowReverse) {
  int count = 0;
  for (int d = 0; d < 4; d++) {
    if (!allowReverse && isReverseDirection(actor, d)) {
      continue;
    }
    if (canEnterTile(actor, d)) {
      dirs[count++] = d;
    }
  }

  if (count == 0 && !allowReverse) {
    return collectLegalDirections(actor, dirs, true);
  }
  return count;
}

int wrappedTileDistanceX(int a, int b) {
  int distance = iabs(a - b);
  int wrapped = GRID_WIDTH - distance;
  return wrapped < distance ? wrapped : distance;
}

int tileDistance(int ax, int ay, int bx, int by) {
  return wrappedTileDistanceX(ax, bx) + iabs(ay - by);
}

int chooseFallbackDirection(const Actor &actor) {
  int current = random(4);
  for (int i = 0; i < 4; i++) {
    if (dirX[i] == actor.dx && dirY[i] == actor.dy) {
      current = i;
      break;
    }
  }
  for (int attempt = 0; attempt < 4; attempt++) {
    int d = (current + attempt) % 4;
    if (canEnterTile(actor, d)) {
      return d;
    }
  }
  return current;
}

int nearestFoodDistanceFrom(int startX, int startY) {
  bool seen[GRID_HEIGHT][GRID_WIDTH];
  int qx[MAX_TILES];
  int qy[MAX_TILES];
  int qd[MAX_TILES];

  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      seen[y][x] = false;
    }
  }

  int head = 0;
  int tail = 0;
  qx[tail] = startX;
  qy[tail] = startY;
  qd[tail] = 0;
  tail++;
  seen[startY][startX] = true;

  while (head < tail) {
    int x = qx[head];
    int y = qy[head];
    int distance = qd[head];
    head++;

    if (pellets[y][x] || powerPellets[y][x]) {
      return distance;
    }

    for (int d = 0; d < 4; d++) {
      int nx = wrappedTileX(x + dirX[d]);
      int ny = y + dirY[d];
      if (ny < 0 || ny >= GRID_HEIGHT || seen[ny][nx] || isWallTile(nx, ny)) {
        continue;
      }
      seen[ny][nx] = true;
      if (tail < MAX_TILES) {
        qx[tail] = nx;
        qy[tail] = ny;
        qd[tail] = distance + 1;
        tail++;
      }
    }
  }

  return GRID_WIDTH + GRID_HEIGHT;
}

int choosePacmanDirection() {
  int dirs[4];
  int legalCount = collectLegalDirections(pacman, dirs, false);
  if (legalCount == 0) {
    return chooseFallbackDirection(pacman);
  }

  int currentTileX = tileForPixel(pacman.x);
  int currentTileY = pacman.y / TILE_SIZE;
  int bestDir = dirs[random(legalCount)];
  int bestScore = -32000;

  for (int i = 0; i < legalCount; i++) {
    int d = dirs[i];
    int nextTileX = wrappedTileX(currentTileX + dirX[d]);
    int nextTileY = currentTileY + dirY[d];
    int nearestGhost = 999;
    int ghostScore = 0;

    for (int g = 0; g < GHOST_COUNT; g++) {
      int ghostTileX = tileForPixel(ghosts[g].x);
      int ghostTileY = ghosts[g].y / TILE_SIZE;
      int distance = tileDistance(nextTileX, nextTileY, ghostTileX, ghostTileY);
      if (distance < nearestGhost) {
        nearestGhost = distance;
      }
      if (powerTimer > 0) {
        ghostScore += (28 - distance) * 3;
      } else {
        ghostScore += distance * 10;
        if (distance <= 1) {
          ghostScore -= 900;
        } else if (distance <= 3) {
          ghostScore -= 180;
        }
      }
    }

    int foodDistance = nearestFoodDistanceFrom(nextTileX, nextTileY);
    int score = ghostScore - foodDistance * (powerTimer > 0 ? 2 : 6);
    score += random(-8, 9);
    if (dirX[d] == pacman.dx && dirY[d] == pacman.dy) {
      score += 10;
    }
    if (powerPellets[nextTileY][nextTileX] && powerTimer == 0 && nearestGhost < 8) {
      score += 220;
    } else if (pellets[nextTileY][nextTileX]) {
      score += 24;
    }

    if (score > bestScore) {
      bestScore = score;
      bestDir = d;
    }
  }

  return bestDir;
}

int chooseGhostDirection(const Actor &ghost, int ghostIndex) {
  int dirs[4];
  int legalCount = collectLegalDirections(ghost, dirs, false);
  if (legalCount == 0) {
    return chooseFallbackDirection(ghost);
  }

  int randomChance = powerTimer > 0 ? 55 : 28;
  if (random(100) < randomChance) {
    return dirs[random(legalCount)];
  }

  int pacTileX = tileForPixel(pacman.x);
  int pacTileY = pacman.y / TILE_SIZE;
  int targetTileX = pacTileX;
  int targetTileY = pacTileY;

  switch (ghostIndex) {
  case 1:
    targetTileX = wrappedTileX(pacTileX + pacman.dx * 4);
    targetTileY = max(1, min(GRID_HEIGHT - 2, pacTileY + pacman.dy * 4));
    break;
  case 2:
    targetTileX = (frameNumber / 120) % 2 == 0 ? 1 : GRID_WIDTH - 2;
    targetTileY = (frameNumber / 240) % 2 == 0 ? 1 : GRID_HEIGHT - 2;
    break;
  case 3:
    if (tileDistance(tileForPixel(ghost.x), ghost.y / TILE_SIZE, pacTileX, pacTileY) < 7) {
      targetTileX = ghostIndex % 2 == 0 ? 1 : GRID_WIDTH - 2;
      targetTileY = GRID_HEIGHT - 2;
    }
    break;
  default:
    break;
  }

  int bestDir = dirs[0];
  int bestScore = powerTimer > 0 ? -32000 : 32000;
  for (int i = 0; i < legalCount; i++) {
    int d = dirs[i];
    int nextTileX = wrappedTileX(tileForPixel(ghost.x) + dirX[d]);
    int nextTileY = ghost.y / TILE_SIZE + dirY[d];
    int distance = tileDistance(nextTileX, nextTileY, targetTileX, targetTileY);
    int score = distance + random(-2, 3);
    if (dirX[d] == ghost.dx && dirY[d] == ghost.dy) {
      score -= powerTimer > 0 ? 0 : 1;
    }
    if (powerTimer > 0) {
      if (score > bestScore) {
        bestScore = score;
        bestDir = d;
      }
    } else if (score < bestScore) {
      bestScore = score;
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
