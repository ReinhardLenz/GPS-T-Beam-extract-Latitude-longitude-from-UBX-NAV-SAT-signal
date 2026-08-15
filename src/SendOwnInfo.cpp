#include "SendOwnInfo.h"

// We keep a pointer to the GPS serial provided by main.cpp
static HardwareSerial* s_gps = nullptr;

// UBX NAV-PVT parser state
static uint8_t  s_state = 0;
static uint8_t  s_cls = 0, s_id = 0;
static uint16_t s_len = 0;
static uint16_t s_payloadIdx = 0;
static uint8_t  s_payload[92]; // NAV-PVT payload length is 92 bytes
static uint8_t  s_ckA = 0, s_ckB = 0;
static uint8_t  s_rxCkA = 0, s_rxCkB = 0;

// last decoded values (so we can send even if no new frame this loop)
static bool     s_havePvt = false;
static bool     s_valid = false;
static uint8_t  s_fixType = 0;
static double   s_lat = 0.0;
static double   s_lon = 0.0;

static int32_t readI32LE(const uint8_t *p) {
  return (int32_t)(
      ((uint32_t)p[0]) |
      ((uint32_t)p[1] << 8) |
      ((uint32_t)p[2] << 16) |
      ((uint32_t)p[3] << 24)
  );
}

void SendOwnInfo_begin(HardwareSerial& gpsSerial) {
  s_gps = &gpsSerial;
}

// Parse as much as available; update s_havePvt/s_lat/s_lon/s_valid/s_fixType when NAV-PVT arrives
static void parseUbxStream() {
  if (!s_gps) return;

  while (s_gps->available()) {
    uint8_t b = (uint8_t)s_gps->read();

    switch (s_state) {
      case 0: // sync 1
        if (b == 0xB5) s_state = 1;
        break;

      case 1: // sync 2
        if (b == 0x62) s_state = 2;
        else s_state = 0;
        break;

      case 2: // class
        s_cls = b;
        s_ckA = 0; s_ckB = 0;
        s_ckA += b; s_ckB += s_ckA;
        s_state = 3;
        break;

      case 3: // id
        s_id = b;
        s_ckA += b; s_ckB += s_ckA;
        s_state = 4;
        break;

      case 4: // len LSB
        s_len = b;
        s_ckA += b; s_ckB += s_ckA;
        s_state = 5;
        break;

      case 5: // len MSB
        s_len |= ((uint16_t)b << 8);
        s_ckA += b; s_ckB += s_ckA;

        if (s_len > sizeof(s_payload)) { // sanity
          s_state = 0;
          break;
        }

        s_payloadIdx = 0;
        s_state = (s_len == 0) ? 7 : 6;
        break;

      case 6: // payload
        s_payload[s_payloadIdx++] = b;
        s_ckA += b; s_ckB += s_ckA;

        if (s_payloadIdx >= s_len) s_state = 7;
        break;

      case 7: // checksum A
        s_rxCkA = b;
        s_state = 8;
        break;

      case 8: // checksum B
        s_rxCkB = b;

        if (s_rxCkA == s_ckA && s_rxCkB == s_ckB) {
          // NAV-PVT?
          if (s_cls == 0x01 && s_id == 0x07 && s_len >= 32) {
            uint8_t fixType = s_payload[20];
            uint8_t flags   = s_payload[21];

            bool gnssFixOk = (flags & 0x01) != 0;
            bool valid = gnssFixOk && (fixType >= 2);

            int32_t lon_e7 = readI32LE(&s_payload[24]);
            int32_t lat_e7 = readI32LE(&s_payload[28]);

            s_lon = lon_e7 / 1e7;
            s_lat = lat_e7 / 1e7;
            s_fixType = fixType;
            s_valid = valid;
            s_havePvt = true;
          }
        }

        s_state = 0; // restart
        break;
    }
  }
}

OwnInfo prepareAndSendOwnInfo(
  SX1262& radio,
  int& transmissionState,
  bool& transmitFlag
) {
  OwnInfo out;

  // Update latest NAV-PVT data from the serial stream
  parseUbxStream();

  out.hasFix = s_havePvt;
  out.valid  = s_valid;
  out.fixType = s_fixType;
  out.lat_proper = s_lat;
  out.lon_proper = s_lon;

  // Build payload (you can change formatting as you like)
  if (out.hasFix) {
    char buf[96];
    snprintf(
      buf, sizeof(buf),
      "LAT=%.7f LON=%.7f valid=%s fixType=%u\r\n",
      out.lat_proper,
      out.lon_proper,
      out.valid ? "true" : "false",
      (unsigned)out.fixType
    );
    out.payload = buf;
  } else {
    out.payload = "No NAV-PVT\r\n";
  }

  // Transmit
  transmissionState = radio.startTransmit(out.payload.c_str());
  transmitFlag = true;

  return out;
}