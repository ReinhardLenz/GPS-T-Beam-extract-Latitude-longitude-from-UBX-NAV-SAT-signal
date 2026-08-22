#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

// --------------------
// ⚠️  LORA
// --------------------

static const int LORA_NSS  = 18;   // CS
static const int LORA_DIO1 = 33;   // DIO1 (IRQ)
static const int LORA_RST  = 23;   // RESET
static const int LORA_BUSY = 32;   // BUSY

SX1262 radio = SX1262(
    new Module(
        LORA_NSS,
        LORA_DIO1,
        LORA_RST,
        LORA_BUSY
    )
);

static const float LORA_FREQ = 868.0;   // must match sender

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("SX126x Sender starting...");
  SPI.begin(5, 19, 27, 18);

  int state = radio.begin(LORA_FREQ);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("radio.begin() failed, code = ");
    Serial.println(state);
    while (true) { delay(1000); }
  }
  Serial.println("✅ Radio init OK");

  // Optional: must match sender if you changed them there
  // radio.setSpreadingFactor(7);
  // radio.setBandwidth(125.0);
  // radio.setCodingRate(5);

  Serial.println("✅ Radio init OK");
  Serial.println("Listening...");
}

void loop() {
  String str;
  int state = radio.receive(str);

  if (state == RADIOLIB_ERR_NONE) {
    //Serial.print("✅ RX: ");
    Serial.println(str);


  } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
    // normal if nothing received (depending on RadioLib settings)
  } else {
    Serial.print("❌ RX failed, code = ");
    Serial.println(state);
  }
}