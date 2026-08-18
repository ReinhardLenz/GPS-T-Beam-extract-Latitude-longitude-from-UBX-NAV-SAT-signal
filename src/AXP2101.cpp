#include "AXP2101.h"
#include <Wire.h>

#define I2C_SDA 21
#define I2C_SCL 22

// AXP2101 PMIC
#define AXP2101_ADDR      0x34
#define AXP2101_DLDO1_VOL 0x99
#define AXP2101_DLDO_EN   0x9C

// ------------------------------------------------------------
// AXP2101 helpers (private to this translation unit)
// ------------------------------------------------------------
static void axpWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(AXP2101_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static uint8_t axpRead(uint8_t reg) {
  Wire.beginTransmission(AXP2101_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)AXP2101_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

static void enableGPSPower() {
  // DLDO1 = 3.3 V
  axpWrite(AXP2101_DLDO1_VOL, 0x1C);
  delay(20);

  // Enable DLDO1, preserving other bits
  uint8_t reg = axpRead(AXP2101_DLDO_EN);
  axpWrite(AXP2101_DLDO_EN, reg | 0x01);
  delay(200);
}

void AXP2101_beginAndEnableGPSPower() {
  // Power GPS via AXP2101
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(50);

  Wire.beginTransmission(AXP2101_ADDR);
  if (Wire.endTransmission() == 0) {
    Serial.println("\nAXP2101 detected -> enabling GPS power...");
    enableGPSPower();
    Serial.println("GPS power enabled.");
  } else {
    Serial.println("\nWARNING: AXP2101 not detected at 0x34.");
    Serial.println("GPS may be unpowered.");
  }

  delay(1000);
}