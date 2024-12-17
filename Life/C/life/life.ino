
#include <Adafruit_Protomatter.h>
#include <Wire.h>

uint8_t rgbPins[]  = {7, 8, 9, 10, 11, 12};
uint8_t addrPins[] = {17, 18, 19, 20, 21};
uint8_t clockPin   = 14;
uint8_t latchPin   = 15;
uint8_t oePin      = 16;

// Matrix configuration
#define MATRIX_WIDTH 128  // Adjust for your LED matrix size
#define MATRIX_HEIGHT 64  // Adjust for your LED matrix size (but see below for address line value)
#define DELAY_TIME 0     // Number of mSec to delay between loops
#define MAX_GENCOUNT 500 // How many generations to get to before resetting to prevent "dead screen"

// 5th argument is number of address lines for "height": 3 is 16, 4 is 32, 5 is 64, 6 is 128
Adafruit_Protomatter matrix(MATRIX_WIDTH, 4, 1, rgbPins, 5, addrPins, clockPin, latchPin, oePin, false);

const int rows = MATRIX_HEIGHT;
const int cols = MATRIX_WIDTH;
int redPixel;
int greenPixel;
int bluePixel;
int generationCount;
bool grid[rows][cols];
bool nextGrid[rows][cols];

// Function to initialize the grid randomly
void initializeGrid() {
  generationCount = 0;
  randomSeed(millis());
  
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      grid[y][x] = random(2); // Randomly alive (1) or dead (0)
    }
  }

  // Set a random color tuple, but make sure it's not black.
  setColors:
    redPixel = random(256);
    greenPixel = random(256);
    bluePixel = random(256);
    if (!redPixel && !greenPixel && !bluePixel)
      goto setColors;
}

// Count live neighbors for a cell
int countNeighbors(int y, int x) {
  int count = 0;
  for (int i = -1; i <= 1; i++) {
    for (int j = -1; j <= 1; j++) {
      if (i == 0 && j == 0) continue; // Skip the cell itself
      int newY = (y + i + rows) % rows;
      int newX = (x + j + cols) % cols;
      if (grid[newY][newX]) count++;
    }
  }
  return count;
}

// Update the grid based on Game of Life rules
void updateGrid() {
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      int neighbors = countNeighbors(y, x);
      if (grid[y][x]) {
        nextGrid[y][x] = (neighbors == 2 || neighbors == 3);
      } else {
        nextGrid[y][x] = (neighbors == 3);
      }
    }
  }
  
  // Copy nextGrid to grid
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      grid[y][x] = nextGrid[y][x];
    }
  }
}

// Draw the grid on the matrix
void drawGrid() {
  matrix.fillScreen(0); // Clear matrix
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      if (grid[y][x]) {
        matrix.drawPixel(x, y, matrix.color565(redPixel, greenPixel, bluePixel)); // Something random for live cells
      }
    }
  }
  matrix.show();
}

void setup(void) {
  Serial.begin(115200);

  // Initialize matrix...
  ProtomatterStatus status = matrix.begin();
  Serial.print("Protomatter begin() status: ");
  Serial.println((int)status);
  if(status != PROTOMATTER_OK) {
    for(;;);
  }
  initializeGrid();
}

void loop() {
  drawGrid();    // Display the current grid
  updateGrid();  // Update the grid for the next iteration
  Serial.println(generationCount);
  delay(DELAY_TIME);    // Adjust delay for speed
  if (generationCount++ >= MAX_GENCOUNT)
    initializeGrid();
}
