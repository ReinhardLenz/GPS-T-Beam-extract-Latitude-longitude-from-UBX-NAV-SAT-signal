#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RadioLib.h>
#include "SendOwnInfo.h"

#define I2C_SDA 21
#define I2C_SCL 22

// GPS UART
HardwareSerial GPSSerial(1);
static const int GPS_RX_PIN = 34;   // GPS TX -> MCU RX
static const int GPS_TX_PIN = 12;   // GPS RX -> MCU TX

// AXP2101 PMIC
#define AXP2101_ADDR      0x34
#define AXP2101_DLDO1_VOL 0x99
#define AXP2101_DLDO_EN   0x9C

// LoRa pins
static const int LORA_NSS  = 18;
static const int LORA_DIO1 = 33;
static const int LORA_RST  = 23;
static const int LORA_BUSY = 32;

static const float LORA_FREQ = 868.0;

SX1262 radio = SX1262(new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY));

int  transmissionState = RADIOLIB_ERR_NONE;
bool transmitFlag = false;

// optional: keep last known good fix
double lastLatProper = 0.0;
double lastLonProper = 0.0;
bool   lastProperFixValid = false;

// ------------------------------------------------------------
// AXP2101 helpers
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

// ------------------------------------------------------------
// UBX helpers (config)
// ------------------------------------------------------------
static void flushGpsInput(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    while (GPSSerial.available()) (void)GPSSerial.read();
    delay(1);
  }
}

static void sendUBX(const uint8_t* msg, uint16_t len) {
  GPSSerial.write(msg, len);
  GPSSerial.flush();
}

static void ubxChecksum(const uint8_t* data, uint16_t len, uint8_t &ckA, uint8_t &ckB) {
  ckA = 0; ckB = 0;
  for (uint16_t i = 0; i < len; i++) {
    ckA = ckA + data[i];
    ckB = ckB + ckA;
  }
}

enum AckResult { ACK_OK, ACK_NAK, ACK_TIMEOUT };

static AckResult waitForAck(uint8_t cls, uint8_t id, uint32_t timeoutMs) {
  uint8_t buf[10];
  uint8_t idx = 0;
  uint32_t start = millis();

  while (millis() - start < timeoutMs) {
    while (GPSSerial.available()) {
      uint8_t b = GPSSerial.read();

      if (idx == 0 && b != 0xB5) continue;
      if (idx == 1 && b != 0x62) { idx = 0; continue; }

      buf[idx++] = b;

      if (idx == 10) {
        idx = 0;

        if (buf[0] != 0xB5 || buf[1] != 0x62 || buf[2] != 0x05) continue;
        if (!((buf[3] == 0x01) || (buf[3] == 0x00))) continue;
        if (buf[4] != 0x02 || buf[5] != 0x00) continue;

        uint8_t ckA, ckB;
        ubxChecksum(&buf[2], 6, ckA, ckB);
        if (ckA != buf[8] || ckB != buf[9]) continue;

        if (buf[6] == cls && buf[7] == id) {
          return (buf[3] == 0x01) ? ACK_OK : ACK_NAK;
        }
      }
    }
    delay(1);
  }
  return ACK_TIMEOUT;
}

static void sendUBX_CFG_MSG(uint8_t targetMsgClass, uint8_t targetMsgId, uint8_t rateUART1) {
  uint8_t payload[8] = {
    targetMsgClass, targetMsgId,
    0,         // rateI2C
    rateUART1, // rateUART1
    0,         // rateUART2
    0,         // rateUSB
    0,         // rateSPI
    0          // reserved
  };

  uint8_t msg[16];
  msg[0] = 0xB5; msg[1] = 0x62;
  msg[2] = 0x06; msg[3] = 0x01; // CFG-MSG
  msg[4] = 0x08; msg[5] = 0x00; // length=8
  memcpy(&msg[6], payload, 8);

  uint8_t ckA, ckB;
  ubxChecksum(&msg[2], 12, ckA, ckB); // class..payload
  msg[14] = ckA; msg[15] = ckB;

  sendUBX(msg, sizeof(msg));
}

void setup() {
  Serial.begin(115200);

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

  // GPS UART
  GPSSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(200);

  // Configure u-blox: disable NMEA, enable UBX-NAV-PVT on UART1
  flushGpsInput(200);

  sendUBX_CFG_MSG(0xF0, 0x00, 0); waitForAck(0x06, 0x01, 2000); flushGpsInput(200); // GGA
  sendUBX_CFG_MSG(0xF0, 0x01, 0); waitForAck(0x06, 0x01, 2000); flushGpsInput(200); // GLL
  sendUBX_CFG_MSG(0xF0, 0x02, 0); waitForAck(0x06, 0x01, 2000); flushGpsInput(200); // GSA
  sendUBX_CFG_MSG(0xF0, 0x03, 0); waitForAck(0x06, 0x01, 2000); flushGpsInput(200); // GSV
  sendUBX_CFG_MSG(0xF0, 0x04, 0); waitForAck(0x06, 0x01, 2000); flushGpsInput(200); // RMC
  sendUBX_CFG_MSG(0xF0, 0x05, 0); waitForAck(0x06, 0x01, 2000); flushGpsInput(200); // VTG

  // Enable NAV-PVT (class 0x01 id 0x07) at rate 1 on UART1
  sendUBX_CFG_MSG(0x01, 0x07, 1);
  waitForAck(0x06, 0x01, 2000);
  flushGpsInput(200);

  // LoRa init
  Serial.println("SX126x Sender starting...");
  SPI.begin(5, 19, 27, 18);

  int state = radio.begin(LORA_FREQ);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("radio.begin() failed, code = ");
    Serial.println(state);
    while (true) { delay(1000); }
  }
  Serial.println("✅ Radio init OK");

  // Tell SendOwnInfo which serial to parse UBX from
  SendOwnInfo_begin(GPSSerial);
}

void loop() {
  // Parse GPS stream + send latest info via LoRa
  OwnInfo own = prepareAndSendOwnInfo(radio, transmissionState, transmitFlag);

  lastProperFixValid = own.valid;
  if (own.valid) {
    lastLatProper = own.lat_proper;
    lastLonProper = own.lon_proper;
    Serial.print("✅ OwnInfo sent: lat=");
    Serial.print(own.lat_proper, 6);
    Serial.print(", lon=");
    Serial.print(own.lon_proper, 6);
    Serial.print(", valid=");
    Serial.println(own.valid);
  }

  // optional pacing
  delay(200);
}