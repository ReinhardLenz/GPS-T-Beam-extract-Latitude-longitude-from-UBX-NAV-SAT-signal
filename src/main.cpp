#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include "SendOwnInfo.h"
#include "AXP2101.h"
#include "U-blox-helper.h"

int sats = 0;
char msg[96];

GpsInfo companion;                 // last parsed companion data (persists)
bool   haveCompanionFix = false;   // indicates we have valid parsed data at least once
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

int transmissionState = RADIOLIB_ERR_NONE;

// flag to indicate transmission or reception state
bool transmitFlag = false;
// flag to indicate that a packet was sent or received
volatile bool operationDone = false;

#define INITIATING_NODE

void setFlag(void) {
  operationDone = true;
}

// Parse received payload into the unified struct
bool parseGpsPayload(const String& str, GpsInfo& in) {
  char buf[128];
  str.toCharArray(buf, sizeof(buf));

  char validStr[6] = {0}; // "true" or "false" (+ null)

  int n = sscanf(
    buf,
    "LAT=%lf LON=%lf valid=%5s fixType=%hhu",
    &in.lat,
    &in.lon,
    validStr,
    &in.fixType
  );

  if (n != 4) {
    return false;
  }

  in.hasData = true;
  in.valid = (strcmp(validStr, "true") == 0);
  return true;
}

void setup() {
  Serial.begin(115200);

  // Power GPS via AXP2101
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

  radio.setDio1Action(setFlag);

#if defined(INITIATING_NODE)
  Serial.print(F("[SX1262] Sending first packet ... "));
  transmissionState = radio.startTransmit("start transmitting");
  transmitFlag = true;
#else
  Serial.print(F("[SX1262] Starting to listen ... "));
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }
#endif
}

void loop() {
  if (operationDone) {
    operationDone = false;

    if (transmitFlag) {
      // previous operation was transmission
      if (transmissionState == RADIOLIB_ERR_NONE) {
        Serial.println(F("transmission finished!"));
      } else {
        Serial.print(F("failed, code "));
        Serial.println(transmissionState);
      }

      // listen for response
      radio.startReceive();
      transmitFlag = false;

    } else {
      // previous operation was reception
      String str;
      int state = radio.readData(str);

      if (state == RADIOLIB_ERR_NONE) {
        if (parseGpsPayload(str, companion)) {
          haveCompanionFix = true;
        } else {
          Serial.println("❌ Failed to parse companion payload");
        }
      }

      delay(1000);

      // send own info
      GpsInfo own = prepareAndSendOwnInfo(radio, transmissionState, transmitFlag);


      if (haveCompanionFix && companion.hasData) {
        Serial.print(companion.lat, 6);
        Serial.print(", ");
        Serial.println(companion.lon, 6);
      } else {
        Serial.println("Companion: (no data yet)");
      }

      Serial.print(own.lat, 6);
      Serial.print(", ");
      Serial.println(own.lon, 6);
      transmitFlag = true;

      
    }
  }
}