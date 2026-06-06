#pragma once

#include "Arduino.h"

#define WL_IDLE_STATUS 0
#define WL_NO_SSID_AVAIL 1
#define WL_CONNECTED 3
#define WL_CONNECT_FAILED 4
#define WL_DISCONNECTED 6

class IPAddress {
public:
  IPAddress(uint8_t a = 127, uint8_t b = 0, uint8_t c = 0, uint8_t d = 1)
      : octets_{a, b, c, d} {}

  uint8_t operator[](int index) const {
    return octets_[index];
  }

private:
  uint8_t octets_[4];
};

class WiFiClass {
public:
  int begin(const char *ssid) {
    (void)ssid;
    status_ = WL_CONNECTED;
    return status_;
  }

  int begin(const char *ssid, const char *password) {
    (void)ssid;
    (void)password;
    status_ = WL_CONNECTED;
    return status_;
  }

  int status() const {
    return status_;
  }

  const char *SSID() const {
    return "simulated-wifi";
  }

  IPAddress localIP() const {
    return IPAddress(127, 0, 0, 1);
  }

  long RSSI() const {
    return -42;
  }

  void end() {
    status_ = WL_DISCONNECTED;
  }

private:
  int status_ = WL_CONNECTED;
};

inline WiFiClass WiFi;
