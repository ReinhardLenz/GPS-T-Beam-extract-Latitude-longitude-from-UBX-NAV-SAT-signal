#pragma once
#include <Arduino.h>
#include <RadioLib.h>

// What we extract from UBX-NAV-PVT and send
struct OwnInfo {
  bool   hasFix     = false;  // true when we decoded a NAV-PVT frame
  bool   valid      = false;  // gnssFixOk && fixType>=2
  uint8_t fixType   = 0;

  double lat_proper = 0.0;
  double lon_proper = 0.0;

  String payload;             // what we sent
};

// Provide the GPS serial instance to the module (call once in setup)
void SendOwnInfo_begin(HardwareSerial& gpsSerial);

// Call repeatedly from loop: parses UBX stream and transmits payload
OwnInfo prepareAndSendOwnInfo(
  SX1262& radio,
  int& transmissionState,
  bool& transmitFlag
);