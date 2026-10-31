#include "WaterQuality.h"

#include <math.h>

namespace pond {

float notANumber() { return __builtin_nanf(""); }

float adcToVolts(float counts, float vref) { return counts * vref / 1024.0f; }

float phFromVoltage(float volts, float v1, float ph1, float v2, float ph2) {
  const float dv = v2 - v1;
  if (dv == 0.0f || isNan(dv) || isNan(volts)) return notANumber();
  return ph1 + (volts - v1) * (ph2 - ph1) / dv;
}

static float tdsPolynomial(float v) { return 133.42f * v * v * v - 255.86f * v * v + 857.39f * v; }

static float temperatureFactor(float tempC) { return 1.0f + 0.02f * (tempC - 25.0f); }

float tdsFromVoltage(float volts, float tempC, float kValue) {
  if (isNan(volts) || isNan(tempC) || isNan(kValue)) return notANumber();
  const float factor = temperatureFactor(tempC);
  if (!(factor > 0.0f)) return notANumber();
  return kValue * tdsPolynomial(volts) / factor * 0.5f;
}

float tdsKValueFromStandard(float volts, float tempC, float standardPpm) {
  const float poly = tdsPolynomial(volts);
  if (!(poly > 0.0f) || !(standardPpm > 0.0f) || isNan(tempC)) return notANumber();
  const float k = (standardPpm / 0.5f) * temperatureFactor(tempC) / poly;
  if (!(k >= 0.25f && k <= 4.0f)) return notANumber();
  return k;
}

float turbidityFromVoltage(float volts, float vClear, float vStd, float ntuStd) {
  const float span = vClear - vStd;
  if (!(span > 0.0f) || isNan(volts) || isNan(ntuStd)) return notANumber();
  const float ntu = (vClear - volts) * ntuStd / span;
  return ntu < 0.0f ? 0.0f : ntu;
}

float doSaturationMgL(float tempC) {
  if (isNan(tempC)) return notANumber();
  // ln C = -139.34411 + 1.575701e5/T - 6.642308e7/T^2 + 1.243800e10/T^3 - 8.621949e11/T^4
  const float inv = 1.0f / (tempC + 273.15f);
  const float lnC =
      -139.34411f + inv * (1.575701e5f + inv * (-6.642308e7f + inv * (1.243800e10f + inv * -8.621949e11f)));
  return expf(lnC);
}

float doSaturationVoltage(float tempC, float v1, float t1, float v2, float t2, uint8_t points) {
  if (isNan(tempC)) return notANumber();
  if (points >= 2 && t1 != t2) return v2 + (tempC - t2) * (v1 - v2) / (t1 - t2);
  if (points >= 1) return v1 + 0.035f * (tempC - t1);
  return notANumber();
}

float doFromVoltage(float volts, float tempC, float vSat) {
  if (!(vSat > 0.0f) || isNan(volts) || isNan(tempC)) return notANumber();
  return volts * doSaturationMgL(tempC) / vSat;
}

}  // namespace pond
