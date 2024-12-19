
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
#define DELAY_TIME 5     // Number of mSec to delay between loops
#define MAX_GENCOUNT 500 // How many generations to get to before resetting to prevent "dead screen"

#if MATRIX_HEIGHT == 16
#define NUM_ADDR_PINS 3
#elif MATRIX_HEIGHT == 32
#define NUM_ADDR_PINS 4
#elif MATRIX_HEIGHT == 64
#define NUM_ADDR_PINS 5
#endif

Adafruit_Protomatter matrix(MATRIX_WIDTH, 1, 1, rgbPins, NUM_ADDR_PINS, addrPins, clockPin, latchPin, oePin, false);

int random_color;
int generation_count;
bool grid[MATRIX_HEIGHT][MATRIX_WIDTH];
bool next_grid[MATRIX_HEIGHT][MATRIX_WIDTH];

typedef struct {
    const char *name;  // Color name
    uint8_t r;         // Red component (0-255)
    uint8_t g;         // Green component (0-255)
    uint8_t b;         // Blue component (0-255)
} PantoneColor;

// Pantone color palette
PantoneColor pantonePalette[] = {
    {"PANTONE 186 C", 255, 0, 0},       // Red
    {"PANTONE 158 C", 255, 128, 0},     // Orange
    {"PANTONE 116 C", 255, 255, 0},     // Yellow
    {"PANTONE 348 C", 0, 128, 0},       // Green
    {"PANTONE 299 C", 0, 128, 255},     // Sky Blue
    {"PANTONE 274 C", 0, 0, 128},       // Deep Blue
    {"PANTONE 527 C", 128, 0, 128},     // Purple
    {"PANTONE 429 C", 192, 192, 192},   // Light Gray
    {"PANTONE Cool Gray 11 C", 128, 128, 128}, // Dark Gray
    {"PANTONE 426 C", 0, 0, 0},         // Black
    {"PANTONE White", 255, 255, 255}    // White
};
#define PALETTE_SIZE (sizeof(pantonePalette) / sizeof(pantonePalette[0]))

// Function to initialize the grid randomly
void intializeEverything() {
  randomSeed(millis());
  random_color = random(PALETTE_SIZE);
  generation_count = 0;

  // Initialize the life board to random stuff
  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      grid[y][x] = random(2); // Randomly alive (1) or dead (0)
    }
  }
}

// Count live neighbors for a cell
int countNeighbors(int y, int x) {
  int count = 0;
  for (int i = -1; i <= 1; i++) {
    for (int j = -1; j <= 1; j++) {
      if (i == 0 && j == 0) continue; // Skip the cell itself
      int newY = (y + i + MATRIX_HEIGHT) % MATRIX_HEIGHT;
      int newX = (x + j + MATRIX_WIDTH) % MATRIX_WIDTH;
      if (grid[newY][newX]) count++;
    }
  }
  return count;
}

// Update the grid based on Game of Life rules
void updateGrid() {
  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      int neighbors = countNeighbors(y, x);
      if (grid[y][x]) {
        next_grid[y][x] = (neighbors == 2 || neighbors == 3);
      } else {
        next_grid[y][x] = (neighbors == 3);
      }
    }
  }
  
  // Copy nextGrid to grid
  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      grid[y][x] = next_grid[y][x];
    }
  }
}

// Draw the grid on the matrix
void drawGrid() {
  matrix.fillScreen(0); // Clear matrix
  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      if (grid[y][x]) {
        // Live cell - draw in random pantone color
        matrix.drawPixel(x, y, matrix.color565(pantonePalette[random_color].r, pantonePalette[random_color].g, pantonePalette[random_color].b));
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
  if (status != PROTOMATTER_OK) {
    Serial.println("Aiee, protomatter is screwed!  %d", status);
    return;
  }
  intializeEverything();
}

void loop() {
  drawGrid();    // Display the current grid
  updateGrid();  // Update the grid for the next iteration
  Serial.println(generation_count);
  delay(DELAY_TIME);    // Adjust delay for speed
  if (generation_count++ >= MAX_GENCOUNT)
    intializeEverything();
}
