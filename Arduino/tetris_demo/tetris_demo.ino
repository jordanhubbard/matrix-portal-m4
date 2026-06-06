#include "SignDisplay.h"

#define BOARD_W 10
#define BOARD_H 8
#define BLOCK 3

uint8_t board[BOARD_H][BOARD_W];
int pieceX = 3;
int pieceY = 0;
int piece = 0;
int rot = 0;
uint16_t pieceColor;

const uint16_t pieces[7][4] = {
  {0x0F00, 0x2222, 0x00F0, 0x4444},
  {0x8E00, 0x6440, 0x0E20, 0x44C0},
  {0x2E00, 0x4460, 0x0E80, 0xC440},
  {0x6600, 0x6600, 0x6600, 0x6600},
  {0x6C00, 0x4620, 0x06C0, 0x4620},
  {0x4E00, 0x4640, 0x0E40, 0x4C40},
  {0xC600, 0x2640, 0x0C60, 0x2640}
};

bool pieceCell(int px, int py, int kind, int r) {
  return (pieces[kind][r & 3] & (0x8000 >> (py * 4 + px))) != 0;
}

bool collision(int nx, int ny, int nr) {
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      if (!pieceCell(x, y, piece, nr)) continue;
      int bx = nx + x;
      int by = ny + y;
      if (bx < 0 || bx >= BOARD_W || by >= BOARD_H) return true;
      if (by >= 0 && board[by][bx]) return true;
    }
  }
  return false;
}

void spawnPiece() {
  piece = random(7);
  rot = random(4);
  pieceX = 3;
  pieceY = -1;
  pieceColor = signWheel((uint8_t)random(255));
  if (collision(pieceX, pieceY, rot)) {
    memset(board, 0, sizeof(board));
  }
}

void lockPiece() {
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      if (!pieceCell(x, y, piece, rot)) continue;
      int bx = pieceX + x;
      int by = pieceY + y;
      if (bx >= 0 && bx < BOARD_W && by >= 0 && by < BOARD_H) {
        board[by][bx] = 1 + piece;
      }
    }
  }
  for (int y = BOARD_H - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < BOARD_W; x++) {
      if (!board[y][x]) full = false;
    }
    if (full) {
      for (int yy = y; yy > 0; yy--) {
        for (int x = 0; x < BOARD_W; x++) board[yy][x] = board[yy - 1][x];
      }
      for (int x = 0; x < BOARD_W; x++) board[0][x] = 0;
      y++;
    }
  }
  spawnPiece();
}

void setup() {
  signBegin();
  randomSeed(analogRead(A0) + millis());
  memset(board, 0, sizeof(board));
  spawnPiece();
}

void loop() {
  if ((millis() / 180) % 2 == 0) {
    int target = (millis() / 1300) % (BOARD_W - 3);
    if (pieceX < target && !collision(pieceX + 1, pieceY, rot)) pieceX++;
    if (pieceX > target && !collision(pieceX - 1, pieceY, rot)) pieceX--;
  }
  if ((millis() / 900) % 4 == 0 && !collision(pieceX, pieceY, rot + 1)) {
    rot = (rot + 1) & 3;
  }
  if (!collision(pieceX, pieceY + 1, rot)) {
    pieceY++;
  } else {
    lockPiece();
  }

  matrix.fillScreen(0);
  int ox = (SIGN_WIDTH - BOARD_W * BLOCK) / 2;
  int oy = 3;
  matrix.drawRect(ox - 1, oy - 1, BOARD_W * BLOCK + 2, BOARD_H * BLOCK + 2, signRgb(50, 80, 120));
  for (int y = 0; y < BOARD_H; y++) {
    for (int x = 0; x < BOARD_W; x++) {
      if (board[y][x]) {
        matrix.fillRect(ox + x * BLOCK, oy + y * BLOCK, BLOCK - 1, BLOCK - 1, signWheel(board[y][x] * 28));
      }
    }
  }
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      if (pieceCell(x, y, piece, rot)) {
        int by = pieceY + y;
        if (by >= 0) matrix.fillRect(ox + (pieceX + x) * BLOCK, oy + by * BLOCK, BLOCK - 1, BLOCK - 1, pieceColor);
      }
    }
  }
  signDrawText(2, 4, "TETRIS", signRgb(255, 230, 100), 1);
  matrix.show();
  delay(95);
}
