// Simple rainbow effect

#include <Adafruit_Protomatter.h>

uint8_t rgbPins[]  = {7, 8, 9, 10, 11, 12};
uint8_t addrPins[] = {17, 18, 19, 20, 21};
uint8_t clockPin   = 14;
uint8_t latchPin   = 15;
uint8_t oePin      = 16;

// This is my dual 64x64 display (tiled left to right)
#define HEIGHT 64
#define WIDTH 128

#if HEIGHT == 16
#define NUM_ADDR_PINS 3
#elif HEIGHT == 32
#define NUM_ADDR_PINS 4
#elif HEIGHT == 64
#define NUM_ADDR_PINS 5
#endif

Adafruit_Protomatter matrix(
  WIDTH, 4, 1, rgbPins, NUM_ADDR_PINS, addrPins, clockPin, latchPin, oePin, false);

// Variables for the waterfall animation
float hueOffset = 0.0;           // Global hue offset (shifts over time)
float waterfallSpeed = 0.01;     // How fast the pattern scrolls downward
float verticalHueFreq = 0.01;    // How much hue changes per row

// Forward declaration of an HSV-to-RGB helper function
void hsvToRgb(float h, float s, float v, uint8_t &r, uint8_t &g, uint8_t &b);

void setup() {
	Serial.begin(9600);

	// Initialize matrix...
	ProtomatterStatus status = matrix.begin();
	if (status != PROTOMATTER_OK) {
		Serial.printf("Aiee, protomatter is screwed!  %d", status);
		return;
	}
	// Clear the display once at startup
	matrix.fillScreen(0);
	matrix.show();
}

void loop() {
	// Render each row with a distinct hue, then shift the entire pattern
	// to create a waterfall motion.
	for(int y = 0; y < HEIGHT; y++) {
		// Calculate this row's hue, wrapping around [0,1)
		float rowHue = fmod(hueOffset + (y * verticalHueFreq), 1.0);

		// Convert HSV (rowHue, 1.0 saturation, 1.0 value) to RGB
		uint8_t r, g, b;
		hsvToRgb(rowHue, 1.0, 1.0, r, g, b);

		// Convert 8-bit RGB to 16-bit color for the matrix
		// Protomatter uses the 565 color format
		uint16_t color = matrix.color565(r, g, b);

		// Fill an entire horizontal row with this color
		for(int x = 0; x < WIDTH; x++) {
			matrix.drawPixel(x, y, color);
		}
	}

	// Show the newly drawn frame
	matrix.show();

	// Advance the global hue offset to make the “waterfall” scroll
	hueOffset = fmod(hueOffset + waterfallSpeed, 1.0);

	// Small delay to control frame rate (optional)
	delay(20);  // ~50 FPS
}

/**
 * Converts HSV color to RGB color (8-bit each) in a straightforward manner.
 * h, s, v are in range [0.0, 1.0].
 * r, g, b outputs in range [0..255].
 */
void hsvToRgb(float h, float s, float v, uint8_t &r, uint8_t &g, uint8_t &b) {
	// Wrap h in [0..1)
	h = fmod(h, 1.0);
	if(h < 0) h += 1.0;

	// Edge cases
	if(s < 0.0) s = 0.0;
	if(s > 1.0) s = 1.0;
	if(v < 0.0) v = 0.0;
	if(v > 1.0) v = 1.0;

	int i = (int)(h * 6.0);
	float f = (h * 6.0) - i;
	float p = v * (1.0 - s);
	float q = v * (1.0 - f * s);
	float t = v * (1.0 - (1.0 - f) * s);

	switch(i % 6) {
	case 0: r = (uint8_t)(v * 255); g = (uint8_t)(t * 255); b = (uint8_t)(p * 255); break;
	case 1: r = (uint8_t)(q * 255); g = (uint8_t)(v * 255); b = (uint8_t)(p * 255); break;
	case 2: r = (uint8_t)(p * 255); g = (uint8_t)(v * 255); b = (uint8_t)(t * 255); break;
	case 3: r = (uint8_t)(p * 255); g = (uint8_t)(q * 255); b = (uint8_t)(v * 255); break;
	case 4: r = (uint8_t)(t * 255); g = (uint8_t)(p * 255); b = (uint8_t)(v * 255); break;
	default: // case 5
		r = (uint8_t)(v * 255); g = (uint8_t)(p * 255); b = (uint8_t)(q * 255); break;
	}
}
