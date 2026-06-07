// Basic Matrix Portal drawing test using the IDE-generated panel layout.

#include "SignDisplay.h"

#define HEIGHT SIGN_HEIGHT
#define WIDTH  SIGN_WIDTH

void setup(void) {
  Serial.begin(115200);
  
  // Initialize matrix...
  ProtomatterStatus status = matrix.begin();
  if (status != PROTOMATTER_OK) {
    Serial.printf("Aiee, protomatter is screwed!  %d", status);
    return;
  }
  
  // Make four color bars (red, green, blue, white) with brightness ramp:
  for(int x=0; x<matrix.width(); x++) {
    uint8_t level = (uint32_t)x * 255 / matrix.width();
    matrix.drawPixel(x, matrix.height() - 4, matrix.color565(level, 0, 0));
    matrix.drawPixel(x, matrix.height() - 3, matrix.color565(0, level, 0));
    matrix.drawPixel(x, matrix.height() - 2, matrix.color565(0, 0, level));
    matrix.drawPixel(x, matrix.height() - 1, matrix.color565(level, level, level));
  }
  
  int16_t shapeY = HEIGHT > 40 ? 16 : HEIGHT / 2;
  matrix.drawCircle(WIDTH / 4, shapeY, 8, matrix.color565(255, 0, 0));
  matrix.drawRect(WIDTH / 2 - 9, shapeY - 8, 18, 18, matrix.color565(0, 255, 0));
  matrix.drawTriangle(WIDTH * 3 / 4, shapeY - 9,
                      WIDTH * 3 / 4 + 9, shapeY + 9,
                      WIDTH * 3 / 4 - 9, shapeY + 9,
                      matrix.color565(0, 0, 255));

  if (WIDTH >= signTextWidth("ADAFRUIT") + 2 && HEIGHT >= 24) {
    signDrawCenteredText(HEIGHT - 14, "ADAFRUIT", matrix.color565(255, 255, 255));
  } else {
    signDrawCenteredText(HEIGHT - 14, "M4", matrix.color565(255, 255, 255));
  }
  
  // AFTER DRAWING, A show() CALL IS REQUIRED TO UPDATE THE MATRIX!
  
  matrix.show(); // Copy data to matrix buffers
}

void loop(void) {
  Serial.print("Refresh FPS = ~");
  Serial.println(matrix.getFrameCount());
  Serial.println(matrix.height());
  Serial.println(matrix.width());
  delay(1000);
}
