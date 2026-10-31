// Radio packet shared by the pond node and the gateways.
// Node and gateways must be built from the same version of this file.
#pragma once

#include <stdint.h>

// ---- Radio settings (edit here only, so both ends always match) -----------
// nRF24 channel n transmits at 2400 + n MHz. The 2.4 GHz ISM band ends at
// 2483.5 MHz (Korea and most other regions), so only channels 0..83 are legal.
// Choose a channel away from the Wi-Fi channel used at the site (docs/hardware.md).
#define POND_RF_CHANNEL 76
// 250 kbps gives the best receiver sensitivity (-94 dBm vs -85 dBm at 1 Mbps).
// Some clone chips do not support it; use RF24_1MBPS on BOTH ends in that case.
#define POND_RF_DATA_RATE RF24_250KBPS
// PA+LNA modules: keep the 3.3 V adapter board and add 10-100 uF across VCC/GND.
#define POND_RF_PA_LEVEL RF24_PA_HIGH
// 5-byte pipe address.
#define POND_RF_ADDRESS "PND01"

namespace pond {

// v1 was the original 16-byte struct of four floats (no header).
const uint8_t PROTOCOL_VERSION = 2;

// Bits of SensorPacket::status.
enum StatusFlag : uint16_t {
  ST_TEMP_INVALID = 1u << 0,
  ST_PH_INVALID = 1u << 1,
  ST_TURB_INVALID = 1u << 2,
  ST_TDS_INVALID = 1u << 3,
  ST_DO_INVALID = 1u << 4,  // also set when no DO sensor is installed
  ST_PH_UNCALIBRATED = 1u << 5,
  ST_TURB_UNCALIBRATED = 1u << 6,
  ST_TDS_UNCALIBRATED = 1u << 7,
  ST_DO_UNCALIBRATED = 1u << 8,
  ST_VCC_UNCALIBRATED = 1u << 9,
  ST_TEMP_FALLBACK = 1u << 10,  // TDS/DO compensated with 25 C (temperature invalid)
  ST_CAL_DEFAULTS = 1u << 11,   // stored calibration missing or corrupt
  ST_PREV_TX_FAILED = 1u << 12, // previous packet was not acknowledged
  ST_AERATOR_CONTROL = 1u << 13 // aerator relay control is enabled
};

const uint8_t AERATOR_UNCONTROLLED = 0xFF;

// Packed so that the 8-bit AVR node and the 32-bit ESP8266 gateway agree on
// the byte layout. Floats are IEEE-754 little-endian on both.
struct __attribute__((packed)) SensorPacket {
  uint8_t version;        // PROTOCOL_VERSION
  uint8_t nodeId;
  uint16_t seq;           // 0 after every node reset, then +1 per packet
  uint16_t status;        // StatusFlag bits
  uint16_t vccMillivolts; // node supply voltage
  float temperatureC;     // NaN when invalid (see status)
  float pH;
  float turbidityNtu;
  float tdsPpm;
  float dissolvedOxygenMgL;
  uint8_t aeratorOn;      // 0/1, or AERATOR_UNCONTROLLED
};

static_assert(sizeof(SensorPacket) <= 32, "nRF24 payloads are limited to 32 bytes");
const uint8_t PACKET_SIZE = sizeof(SensorPacket);

}  // namespace pond
