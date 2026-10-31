#include "Calibration.h"

#include <math.h>
#include <string.h>

#include "WaterQuality.h"

namespace pond {

static bool isFiniteValue(float x) { return !isNan(x) && x < 3.0e38f && x > -3.0e38f; }

void calDefaults(CalibrationData& c, uint8_t phBoardVersion) {
  memset(&c, 0, sizeof(c));
  c.magic = CAL_MAGIC;
  c.layout = CAL_LAYOUT;
  c.phBoardVersion = (phBoardVersion == 2) ? 2 : 1;
  if (c.phBoardVersion == 2) {
    c.phV1 = 1.500f;  // DFRobot_PH defaults (V2 board)
    c.phPh1 = 7.0f;
    c.phV2 = 2.03244f;
    c.phPh2 = 4.0f;
  } else {
    c.phV1 = 2.000f;  // pH = 3.5 * V (V1 board example)
    c.phPh1 = 7.0f;
    c.phV2 = 4.0f / 3.5f;
    c.phPh2 = 4.0f;
  }
  c.tdsK = 1.0f;
  // Same line as the 2024 firmware, NTU = (785.7 - ADC) / 1.22 at a 5.0 V reference.
  // Not a calibration: run "cal ntu0" and "cal ntu <NTU>" (docs/calibration.md).
  c.turbVClear = 785.7f * 5.0f / 1024.0f;
  c.turbNtuStd = 100.0f;
  c.turbVStd = c.turbVClear - c.turbNtuStd * 1.22f * 5.0f / 1024.0f;
  c.tempOffsetC = 0.0f;
  c.bandgapV = 1.1f;  // datasheet nominal, +/-10 % until "cal vcc" is run
  c.doV1 = 1.6f;      // DFRobot SEN0237 example: 1600 mV saturation at 25 C
  c.doT1 = 25.0f;
  c.doV2 = 1.6f;
  c.doT2 = 25.0f;
  c.flags = 0;
  calSeal(c);
}

uint16_t crc16(const uint8_t* data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

void calSeal(CalibrationData& c) {
  c.crc = crc16(reinterpret_cast<const uint8_t*>(&c), offsetof(CalibrationData, crc));
}

bool calIsValid(const CalibrationData& c) {
  if (c.magic != CAL_MAGIC || c.layout != CAL_LAYOUT) return false;
  if (c.crc != crc16(reinterpret_cast<const uint8_t*>(&c), offsetof(CalibrationData, crc))) return false;
  if (c.phBoardVersion != 1 && c.phBoardVersion != 2) return false;
  const float values[] = {c.phV1, c.phPh1, c.phV2, c.phPh2, c.tdsK, c.turbVClear, c.turbVStd, c.turbNtuStd,
                          c.tempOffsetC, c.bandgapV, c.doV1, c.doT1, c.doV2, c.doT2};
  for (uint8_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
    if (!isFiniteValue(values[i])) return false;
  }
  if (c.phV1 == c.phV2) return false;
  if (!(c.tdsK >= 0.25f && c.tdsK <= 4.0f)) return false;
  if (!(c.turbVClear > c.turbVStd) || !(c.turbNtuStd > 0.0f)) return false;
  if (fabsf(c.tempOffsetC) > 5.0f) return false;
  if (!(c.bandgapV >= 1.0f && c.bandgapV <= 1.2f)) return false;
  if (!(c.doV1 > 0.0f)) return false;
  return true;
}

uint8_t calDoPoints(const CalibrationData& c) {
  return ((c.flags & CAL_DO_P1) && (c.flags & CAL_DO_P2)) ? 2 : 1;
}

bool calApplyPhPoint(CalibrationData& c, uint8_t point, float volts, float bufferPh) {
  if (point != 1 && point != 2) return false;
  if (!(volts >= 0.05f && volts <= 4.95f) || !(bufferPh >= 0.0f && bufferPh <= 14.0f)) return false;

  const bool otherSet = (point == 1) ? (c.flags & CAL_PH_P2) != 0 : (c.flags & CAL_PH_P1) != 0;
  float otherV = (point == 1) ? c.phV2 : c.phV1;
  float otherPh = (point == 1) ? c.phPh2 : c.phPh1;
  if (otherSet) {
    if (fabsf(volts - otherV) < 0.05f || fabsf(bufferPh - otherPh) < 1.0f) return false;
  } else {
    // One-point (offset) calibration: keep the current slope, move the line.
    const float slope = (c.phPh2 - c.phPh1) / (c.phV2 - c.phV1);  // pH per volt
    if (!isFiniteValue(slope) || slope == 0.0f) return false;
    otherPh = (bufferPh >= 7.0f) ? bufferPh - 3.0f : bufferPh + 3.0f;
    otherV = volts + (otherPh - bufferPh) / slope;
  }
  if (point == 1) {
    c.phV1 = volts;
    c.phPh1 = bufferPh;
    c.phV2 = otherV;
    c.phPh2 = otherPh;
    c.flags |= CAL_PH_P1;
  } else {
    c.phV2 = volts;
    c.phPh2 = bufferPh;
    c.phV1 = otherV;
    c.phPh1 = otherPh;
    c.flags |= CAL_PH_P2;
  }
  calSeal(c);
  return true;
}

bool calApplyTds(CalibrationData& c, float volts, float tempC, float standardPpm) {
  if (!(standardPpm > 0.0f && standardPpm <= 2000.0f) || !(tempC >= 0.0f && tempC <= 40.0f)) return false;
  const float k = tdsKValueFromStandard(volts, tempC, standardPpm);
  if (isNan(k)) return false;
  c.tdsK = k;
  c.flags |= CAL_TDS;
  calSeal(c);
  return true;
}

bool calApplyTurbidityZero(CalibrationData& c, float volts) {
  if (!(volts >= 0.5f && volts <= 4.95f)) return false;
  if (c.flags & CAL_TURB_STD) {
    if (!(volts - c.turbVStd >= 0.02f)) return false;  // clear water must read above the standard
  } else {
    c.turbVStd = volts - (c.turbVClear - c.turbVStd);  // keep the current NTU-per-volt slope
  }
  c.turbVClear = volts;
  c.flags |= CAL_TURB_ZERO;
  calSeal(c);
  return true;
}

bool calApplyTurbidityStandard(CalibrationData& c, float volts, float ntu) {
  if (!(ntu > 0.0f && ntu <= 4000.0f) || !(volts >= 0.05f && volts <= 4.95f)) return false;
  if (!(c.turbVClear - volts >= 0.02f)) return false;
  c.turbVStd = volts;
  c.turbNtuStd = ntu;
  c.flags |= CAL_TURB_STD;
  calSeal(c);
  return true;
}

bool calApplyTempReference(CalibrationData& c, float rawSensorC, float referenceC) {
  if (isNan(rawSensorC) || !(referenceC > -5.0f && referenceC < 50.0f)) return false;
  const float offset = referenceC - rawSensorC;
  if (fabsf(offset) > 5.0f) return false;
  c.tempOffsetC = offset;
  c.flags |= CAL_TEMP;
  calSeal(c);
  return true;
}

bool calApplyVcc(CalibrationData& c, float bandgapCounts, float actualVcc) {
  if (!(actualVcc >= 4.0f && actualVcc <= 5.5f) || !(bandgapCounts > 0.0f)) return false;
  const float bandgap = actualVcc * bandgapCounts / 1024.0f;
  if (!(bandgap >= 1.0f && bandgap <= 1.2f)) return false;
  c.bandgapV = bandgap;
  c.flags |= CAL_VCC;
  calSeal(c);
  return true;
}

bool calApplyDoPoint(CalibrationData& c, uint8_t point, float volts, float tempC) {
  if (point != 1 && point != 2) return false;
  if (!(volts > 0.01f && volts < 3.4f) || !(tempC >= 0.0f && tempC <= 40.0f)) return false;
  if (point == 2) {
    if (!(c.flags & CAL_DO_P1) || fabsf(tempC - c.doT1) < 5.0f) return false;
    c.doV2 = volts;
    c.doT2 = tempC;
    c.flags |= CAL_DO_P2;
  } else {
    c.doV1 = volts;
    c.doT1 = tempC;
    c.flags |= CAL_DO_P1;
    if ((c.flags & CAL_DO_P2) && fabsf(c.doT2 - tempC) < 5.0f) c.flags &= ~CAL_DO_P2;
  }
  calSeal(c);
  return true;
}

}  // namespace pond
