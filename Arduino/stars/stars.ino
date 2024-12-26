#include <Adafruit_Protomatter.h>
#include <Adafruit_GFX.h>
#include <math.h>

uint8_t rgbPins[]  = {7, 8, 9, 10, 11, 12};
uint8_t addrPins[] = {17, 18, 19, 20, 21};
uint8_t clockPin   = 14;
uint8_t latchPin   = 15;
uint8_t oePin      = 16;

// Matrix configuration
#define WIDTH 128  // Adjust for your LED matrix size
#define HEIGHT 64  // Adjust for your LED matrix size (but see below for address line value)
#define DELAY_TIME 0    // Number of mSec to delay between loops
#define MAX_BRIGHT 255   // Maximum brightness value per pixel
#define NUM_STARS 400	 // How many stars to work with in the array (not necessarily visible!)

#if HEIGHT == 16
#define NUM_ADDR_PINS 3
#elif HEIGHT == 32
#define NUM_ADDR_PINS 4
#elif HEIGHT == 64
#define NUM_ADDR_PINS 5
#endif

Adafruit_Protomatter matrix(WIDTH, 4, 1, rgbPins, NUM_ADDR_PINS, addrPins, clockPin, latchPin, oePin, false);

// Star properties
struct Star {
  float x;
  float y;
  float z;
};
Star stars[NUM_STARS];

// Animation parameters
float speed = 0.2;

void setup() {
  Serial.begin(115200);
  
  // Initialize matrix...
  ProtomatterStatus status = matrix.begin();
  Serial.print("Protomatter begin() status: ");
  Serial.println((int)status);
  if (status != PROTOMATTER_OK) {
    Serial.printf("Aiee, protomatter is screwed!  %d", status);
    return;
  }

  // Initialize star positions
  for (int i = 0; i < NUM_STARS; i++) {
    stars[i].x = random(-WIDTH, WIDTH);
    stars[i].y = random(-HEIGHT, HEIGHT);
    stars[i].z = random(1, WIDTH);
  }
}

void loop() {
  matrix.fillScreen(0); // Clear the screen

  for (int i = 0; i < NUM_STARS; i++) {
    // Project 3D coordinates to 2D
    int screenX = (stars[i].x / stars[i].z) * WIDTH / 2 + WIDTH / 2;
    int screenY = (stars[i].y / stars[i].z) * HEIGHT / 2 + HEIGHT / 2;

    // Draw the star if it's within bounds
    if (screenX >= 0 && screenX < WIDTH && screenY >= 0 && screenY < HEIGHT) {
      int brightness = MAX_BRIGHT - ((stars[i].z * MAX_BRIGHT) / WIDTH);
      uint16_t color = matrix.color565(brightness, brightness, brightness);
      matrix.drawPixel(screenX, screenY, color);
    }

    // Move the star closer
    stars[i].z -= speed;

    // Reset star if it moves off-screen
    if (stars[i].z <= 0) {
      stars[i].x = random(-WIDTH, WIDTH);
      stars[i].y = random(-HEIGHT, HEIGHT);
      stars[i].z = random(1, WIDTH);
    }
  }

  // Show the updated display
  matrix.show();

  // Small delay to control the animation speed
  delay(DELAY_TIME);
}
