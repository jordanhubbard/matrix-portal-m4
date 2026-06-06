#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <string>
#include <type_traits>

#include "MatrixPortalSim.h"

#ifndef TWO_PI
#define TWO_PI 6.28318530717958647692
#endif

#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define HIGH 1
#define LOW 0
#define LED_BUILTIN 13
#define A0 14
#define A1 15
#define A2 16
#define A3 17
#define A4 18
#define BUTTON_UP 2
#define BUTTON_DOWN 3
#define PIN_NEOPIXEL 4
#define NEOPIXEL 4
#define ESP_RESET 30
#define ESP_BUSY 31
#define ESP_CS 33

template <typename A, typename B>
typename std::common_type<A, B>::type min(A a, B b) {
  using Common = typename std::common_type<A, B>::type;
  return a < b ? static_cast<Common>(a) : static_cast<Common>(b);
}

template <typename A, typename B>
typename std::common_type<A, B>::type max(A a, B b) {
  using Common = typename std::common_type<A, B>::type;
  return a > b ? static_cast<Common>(a) : static_cast<Common>(b);
}

void delay(unsigned long ms);
unsigned long millis();
unsigned long micros();
int analogRead(int pin);
void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
int digitalRead(int pin);
void randomSeed(unsigned long seed);
long random(long max);
long random(long min, long max);

class SerialPort {
public:
  void begin(unsigned long baud);

  template <typename T>
  void print(const T &value) {
    std::printf("%s", toString(value).c_str());
  }

  void print(const char *value);
  void print(char value);
  void print(int value);
  void print(unsigned int value);
  void print(long value);
  void print(unsigned long value);
  void print(float value);
  void print(double value);

  template <typename T>
  void println(const T &value) {
    print(value);
    std::printf("\n");
  }

  void println();
  void printf(const char *format, ...);

private:
  static std::string toString(const char *value);
  static std::string toString(char value);
  static std::string toString(int value);
  static std::string toString(unsigned int value);
  static std::string toString(long value);
  static std::string toString(unsigned long value);
  static std::string toString(float value);
  static std::string toString(double value);
};

extern SerialPort Serial;
