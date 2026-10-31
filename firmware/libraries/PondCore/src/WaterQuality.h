// Sensor conversions. Pure functions: no hardware access, unit-tested on the host.
#pragma once

#include <stdint.h>

namespace pond {

// Portable NaN helpers (isnan() differs between the AVR, ESP8266 and host toolchains).
float notANumber();
inline bool isNan(float x) { return x != x; }

// ADC counts (0..1023, may be fractional after averaging) to volts: V = counts * Vref / 1024.
float adcToVolts(float counts, float vref);

// ---- pH ------------------------------------------------------------------
// Straight line through two calibration points (volts, pH). The slope sign comes
// from the points, so one function serves both DFRobot board generations:
//   V1 board ("pH meter V1.1"):          pH = 3.5 * V -> (2.000 V, 7.00), (1.143 V, 4.00)
//   V2 board (SEN0161-V2 / SEN0169-V2):  (1.500 V, 7.00), (2.032 V, 4.00)  (DFRobot_PH defaults)
// Returns NaN if both points have the same voltage.
float phFromVoltage(float volts, float v1, float ph1, float v2, float ph2);

// ---- TDS -----------------------------------------------------------------
// DFRobot GravityTDS method:
//   EC(T)  = k * (133.42 V^3 - 255.86 V^2 + 857.39 V)   [uS/cm]
//   EC(25) = EC(T) / (1 + 0.02 * (T - 25))
//   TDS    = EC(25) * 0.5                                [ppm]
float tdsFromVoltage(float volts, float tempC, float kValue);
// k that makes the probe read `standardPpm` (e.g. 707 ppm = 1413 uS/cm) at `tempC`.
// Returns NaN if k falls outside the plausible range 0.25..4.0.
float tdsKValueFromStandard(float volts, float tempC, float standardPpm);

// ---- Turbidity -----------------------------------------------------------
// Linear between the clear-water voltage (0 NTU) and one standard (vStd, ntuStd).
// Voltage falls as turbidity rises; the result is clamped at 0 NTU.
// Returns NaN if vClear <= vStd.
float turbidityFromVoltage(float volts, float vClear, float vStd, float ntuStd);

// ---- Dissolved oxygen ----------------------------------------------------
// Oxygen solubility in fresh water at 1 atm, mg/L (Benson & Krause 1984, as used by APHA).
float doSaturationMgL(float tempC);
// Probe voltage in air-saturated water at tempC (DFRobot SEN0237 method).
// points == 1: v1 + 35 mV/C * (tempC - t1); points == 2: line through (t1, v1) and (t2, v2).
float doSaturationVoltage(float tempC, float v1, float t1, float v2, float t2, uint8_t points);
// DO = V * saturation(T) / Vsat(T). Returns NaN if vSat <= 0.
float doFromVoltage(float volts, float tempC, float vSat);

}  // namespace pond
