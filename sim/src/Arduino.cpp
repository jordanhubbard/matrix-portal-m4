#include "Arduino.h"

#include "Adafruit_Protomatter.h"

#include <SDL.h>

#include <random>
#include <string>

SerialPort Serial;

namespace {
std::mt19937 rng{1};
int pinModes[64] = {0};

int analogIndex(int pin) {
  switch (pin) {
  case A0: return 0;
  case A1: return 1;
  case A2: return 2;
  case A3: return 3;
  case A4: return 4;
  default: return -1;
  }
}
}

void delay(unsigned long ms) {
  MatrixPortalSim::sleep(ms);
}

unsigned long millis() {
  return SDL_GetTicks();
}

unsigned long micros() {
  static const uint64_t frequency = SDL_GetPerformanceFrequency();
  const uint64_t counter = SDL_GetPerformanceCounter();
  return static_cast<unsigned long>((counter * 1000000ULL) / frequency);
}

int analogRead(int pin) {
  int index = analogIndex(pin);
  if (index >= 0) {
    return MatrixPortalSim::hardware().analogValues[index];
  }
  return static_cast<int>((pin * 97 + millis()) % 1024);
}

void pinMode(int pin, int mode) {
  if (pin >= 0 && pin < static_cast<int>(sizeof(pinModes) / sizeof(pinModes[0]))) {
    pinModes[pin] = mode;
  }
}

void digitalWrite(int pin, int value) {
  if (pin == LED_BUILTIN) {
    MatrixPortalSim::hardware().d13Led = value != LOW;
  }
}

int digitalRead(int pin) {
  if (pin == BUTTON_UP) {
    return MatrixPortalSim::hardware().buttonUpPressed ? LOW : HIGH;
  }
  if (pin == BUTTON_DOWN) {
    return MatrixPortalSim::hardware().buttonDownPressed ? LOW : HIGH;
  }
  if (pin == LED_BUILTIN) {
    return MatrixPortalSim::hardware().d13Led ? HIGH : LOW;
  }
  if (pin == ESP_BUSY) {
    return LOW;
  }
  return HIGH;
}

void randomSeed(unsigned long seed) {
  rng.seed(seed);
}

long random(long max) {
  if (max <= 0) {
    return 0;
  }
  std::uniform_int_distribution<long> dist(0, max - 1);
  return dist(rng);
}

long random(long min, long max) {
  if (max <= min) {
    return min;
  }
  std::uniform_int_distribution<long> dist(min, max - 1);
  return dist(rng);
}

void SerialPort::begin(unsigned long baud) {
  std::printf("[serial] begin %lu baud\n", baud);
}

void SerialPort::print(const char *value) {
  std::printf("%s", value ? value : "");
}

void SerialPort::print(char value) {
  std::printf("%c", value);
}

void SerialPort::print(int value) {
  std::printf("%d", value);
}

void SerialPort::print(unsigned int value) {
  std::printf("%u", value);
}

void SerialPort::print(long value) {
  std::printf("%ld", value);
}

void SerialPort::print(unsigned long value) {
  std::printf("%lu", value);
}

void SerialPort::print(float value) {
  std::printf("%f", static_cast<double>(value));
}

void SerialPort::print(double value) {
  std::printf("%f", value);
}

void SerialPort::println() {
  std::printf("\n");
}

void SerialPort::printf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  std::vprintf(format, args);
  va_end(args);
}

std::string SerialPort::toString(const char *value) {
  return value ? std::string(value) : std::string();
}

std::string SerialPort::toString(char value) {
  return std::string(1, value);
}

std::string SerialPort::toString(int value) {
  return std::to_string(value);
}

std::string SerialPort::toString(unsigned int value) {
  return std::to_string(value);
}

std::string SerialPort::toString(long value) {
  return std::to_string(value);
}

std::string SerialPort::toString(unsigned long value) {
  return std::to_string(value);
}

std::string SerialPort::toString(float value) {
  return std::to_string(value);
}

std::string SerialPort::toString(double value) {
  return std::to_string(value);
}
