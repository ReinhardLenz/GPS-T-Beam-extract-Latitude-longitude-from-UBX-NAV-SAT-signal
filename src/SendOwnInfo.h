#pragma once
#include <Arduino.h>
#include <RadioLib.h>

// Unified GPS info struct used for BOTH own and companion data
struct GpsInfo {
  bool    hasData  = false;  // true when we decoded at least one NAV-PVT frame
  bool    valid    = false;  // gnssFixOk && fixType>=2
  uint8_t fixType  = 0;

  double  lat      = 0.0;
  double  lon      = 0.0;
};

// Provide the GPS serial instance to the module (call once in setup)
void SendOwnInfo_begin(HardwareSerial& gpsSerial);

// Call repeatedly from loop: parses UBX stream and transmits payload.
// Returns the latest decoded GPS info (GpsInfo).
GpsInfo prepareAndSendOwnInfo(
  SX1262& radio,
  int& transmissionState,
  bool& transmitFlag
);