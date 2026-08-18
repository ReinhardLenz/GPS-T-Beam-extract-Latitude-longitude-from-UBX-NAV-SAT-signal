#pragma once
#include <Arduino.h>

// Initializes I2C, detects AXP2101 at 0x34, and enables GPS power (DLDO1=3.3V).
// Prints status messages to Serial (same as your original setup() code).
void AXP2101_beginAndEnableGPSPower();