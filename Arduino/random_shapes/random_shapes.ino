// Simple layout-aware rainbow shapes effect.

#include "SignDisplay.h"

#define HEIGHT SIGN_HEIGHT
#define WIDTH  SIGN_WIDTH

// Variables for animation
float angle = 0;
float scale = 1.0;
int hue = 0;

uint16_t hsvToRgb565(uint8_t h, uint8_t s, uint8_t v);

void setup() {
  Serial.begin(115200);

  // Initialize matrix...
  ProtomatterStatus status = matrix.begin();
  if (status != PROTOMATTER_OK) {
    Serial.printf("Aiee, protomatter is screwed!  %d", status);
    return;
  }
  matrix.fillScreen(0); // Clear the screen
}

void loop() {
  int centerX = WIDTH / 2;
  int centerY = HEIGHT / 2;
  
  // Rotate and scale the shapes
  float scaledSize = 10 + 10 * scale;

  // Draw rectangle
  int rectX = centerX + cos(angle) * scaledSize - scaledSize / 2;
  int rectY = centerY + sin(angle) * scaledSize - scaledSize / 2;
  matrix.fillRect(rectX, rectY, scaledSize, scaledSize, hsvToRgb565(hue, 255, 255));

  // Draw circle
  int circleX = centerX - cos(angle) * scaledSize;
  int circleY = centerY - sin(angle) * scaledSize;
  matrix.fillCircle(circleX, circleY, scaledSize / 2, hsvToRgb565((hue + 85) % 255, 255, 255));

  // Draw a triangle
  int triX = centerX - cos(angle) * scaledSize;
  int triY = centerY - sin(angle) * scaledSize;
  matrix.drawTriangle(triX, triY, rectX + (scaledSize / 2), rectY + (scaledSize / 2), centerX / 2, centerY/ 2, hsvToRgb565((hue + 170) % 255, 255, 255));
  
  // Update animation parameters
  angle += 0.05;
  if (angle > TWO_PI) angle -= TWO_PI;

  scale += 0.05;
  if (scale > 2.0) scale = 0.5;
  
  hue += 2;
  if (hue > 255) hue -= 255;
  matrix.show();
  delay(10);
}

// Convert HSV to RGB565 color
uint16_t hsvToRgb565(uint8_t h, uint8_t s, uint8_t v) {
  uint8_t r, g, b;
  
  uint8_t region = h / 43;
  uint8_t remainder = (h - (region * 43)) * 6;
  
  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
  
  switch (region) {
  case 0:
    r = v; g = t; b = p;
    break;
  case 1:
    r = q; g = v; b = p;
    break;
  case 2:
    r = p; g = v; b = t;
    break;
  case 3:
    r = p; g = q; b = v;
    break;
  case 4:
    r = t; g = p; b = v;
    break;
  default:
    r = v; g = p; b = q;
    break;
  } 
  return matrix.color565(r, g, b);
}
