#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include "SendOwnInfo.h"
#include "AXP2101.h"
#include "U-blox-helper.h"

// GPS UART
HardwareSerial GPSSerial(1);
static const int GPS_RX_PIN = 34;   // GPS TX -> MCU RX
static const int GPS_TX_PIN = 12;   // GPS RX -> MCU TX

// --------------------
// ⚠️  LORA
// --------------------
static const int LORA_NSS  = 18;
static const int LORA_DIO1 = 33;
static const int LORA_RST  = 23;
static const int LORA_BUSY = 32;

SX1262 radio = SX1262(
    new Module(
        LORA_NSS,
        LORA_DIO1,
        LORA_RST,
        LORA_BUSY
    )
);

static const float LORA_FREQ = 868.0;

int  transmissionState = RADIOLIB_ERR_NONE;
bool transmitFlag = false;

// optional: keep last known good fix
double lastLatProper = 0.0;
double lastLonProper = 0.0;
bool   lastProperFixValid = false;

void setup() {
  Serial.begin(115200);

  // Power GPS via AXP2101 (moved to separate module)
  AXP2101_beginAndEnableGPSPower();

  // GPS UART
  GPSSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(200);

  // Provide GPS serial to u-blox helper module and configure receiver output
  UbloxHelper_begin(GPSSerial);

  bool ok = UbloxHelper_configureUbxOnlyNavPvt();
  if (!ok) {
    Serial.println("⚠️ u-blox config: NAV-PVT enable did not ACK (continuing anyway).");
  }

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

  Serial.print(own.lat_proper, 6);
  Serial.print(", ");
  Serial.println(own.lon_proper, 6);

  // optional pacing
  delay(600);
}