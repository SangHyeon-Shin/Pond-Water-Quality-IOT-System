// Calibration constants of the pond node, stored in EEPROM with a CRC.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace pond {

// Which constants were set by the user (others are defaults).
enum CalFlag : uint16_t {
  CAL_PH_P1 = 1u << 0,
  CAL_PH_P2 = 1u << 1,
  CAL_TDS = 1u << 2,
  CAL_TURB_ZERO = 1u << 3,
  CAL_TURB_STD = 1u << 4,
  CAL_TEMP = 1u << 5,
  CAL_VCC = 1u << 6,
  CAL_DO_P1 = 1u << 7,
  CAL_DO_P2 = 1u << 8
};

const uint16_t CAL_MAGIC = 0x504E;  // "PN"
const uint8_t CAL_LAYOUT = 1;       // bump when the struct changes

struct __attribute__((packed)) CalibrationData {
  uint16_t magic;
  uint8_t layout;
  uint8_t phBoardVersion;  // 1 or 2: selects the default pH line
  uint16_t flags;          // CalFlag bits
  float phV1, phPh1, phV2, phPh2;          // two (volts, pH) points
  float tdsK;                              // GravityTDS k value
  float turbVClear, turbVStd, turbNtuStd;  // 0 NTU voltage, standard voltage, standard NTU
  float tempOffsetC;                       // added to the DS18B20 reading
  float bandgapV;                          // measured internal 1.1 V reference (AVR)
  float doV1, doT1, doV2, doT2;            // saturation voltage(s) and temperature(s)
  uint16_t crc;                            // CRC-16/CCITT-FALSE of all bytes before it
};

void calDefaults(CalibrationData& c, uint8_t phBoardVersion);
uint16_t crc16(const uint8_t* data, size_t length);
void calSeal(CalibrationData& c);           // recomputes crc
bool calIsValid(const CalibrationData& c);  // magic, layout, crc and plausibility

inline bool calPhDone(const CalibrationData& c) { return (c.flags & (CAL_PH_P1 | CAL_PH_P2)) == (CAL_PH_P1 | CAL_PH_P2); }
inline bool calTurbidityDone(const CalibrationData& c) {
  return (c.flags & (CAL_TURB_ZERO | CAL_TURB_STD)) == (CAL_TURB_ZERO | CAL_TURB_STD);
}
inline bool calTdsDone(const CalibrationData& c) { return (c.flags & CAL_TDS) != 0; }
inline bool calDoDone(const CalibrationData& c) { return (c.flags & CAL_DO_P1) != 0; }
inline bool calVccDone(const CalibrationData& c) { return (c.flags & CAL_VCC) != 0; }
uint8_t calDoPoints(const CalibrationData& c);

// Each function validates its inputs and returns false, leaving `c` unchanged, when
// they are implausible. When only one point of a two-point sensor is set, the
// current slope is kept and the line is shifted through the new point.
bool calApplyPhPoint(CalibrationData& c, uint8_t point, float volts, float bufferPh);
bool calApplyTds(CalibrationData& c, float volts, float tempC, float standardPpm);
bool calApplyTurbidityZero(CalibrationData& c, float volts);
bool calApplyTurbidityStandard(CalibrationData& c, float volts, float ntu);
bool calApplyTempReference(CalibrationData& c, float rawSensorC, float referenceC);
bool calApplyVcc(CalibrationData& c, float bandgapCounts, float actualVcc);
bool calApplyDoPoint(CalibrationData& c, uint8_t point, float volts, float tempC);

}  // namespace pond
