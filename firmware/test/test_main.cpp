// Host unit tests for the PondCore library (no external dependencies).
// Run: make -C firmware/test
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "PondCore.h"
#include "PondGateway.h"

using namespace pond;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                          \
  do {                                                                       \
    ++g_checks;                                                              \
    if (!(cond)) {                                                           \
      ++g_failures;                                                          \
      printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);        \
    }                                                                        \
  } while (0)

#define CHECK_NEAR(actual, expected, tol)                                                          \
  do {                                                                                             \
    ++g_checks;                                                                                    \
    const double a_ = (actual), e_ = (expected);                                                   \
    if (!(fabs(a_ - e_) <= (tol))) {                                                               \
      ++g_failures;                                                                                \
      printf("%s:%d: CHECK_NEAR failed: %s = %.6f, expected %.6f\n", __FILE__, __LINE__, #actual, \
             a_, e_);                                                                              \
    }                                                                                              \
  } while (0)

#define CHECK_STR(actual, expected)                                                                    \
  do {                                                                                                 \
    ++g_checks;                                                                                        \
    if (strcmp((actual), (expected)) != 0) {                                                           \
      ++g_failures;                                                                                    \
      printf("%s:%d: CHECK_STR failed: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (actual), (expected)); \
    }                                                                                                  \
  } while (0)

static float adc(int counts) { return adcToVolts(static_cast<float>(counts), 5.0f); }

static void testProtocol() {
  CHECK(sizeof(SensorPacket) == 29);
  CHECK(offsetof(SensorPacket, version) == 0);
  CHECK(offsetof(SensorPacket, nodeId) == 1);
  CHECK(offsetof(SensorPacket, seq) == 2);
  CHECK(offsetof(SensorPacket, status) == 4);
  CHECK(offsetof(SensorPacket, vccMillivolts) == 6);
  CHECK(offsetof(SensorPacket, temperatureC) == 8);
  CHECK(offsetof(SensorPacket, pH) == 12);
  CHECK(offsetof(SensorPacket, turbidityNtu) == 16);
  CHECK(offsetof(SensorPacket, tdsPpm) == 20);
  CHECK(offsetof(SensorPacket, dissolvedOxygenMgL) == 24);
  CHECK(offsetof(SensorPacket, aeratorOn) == 28);
}

static void testPh() {
  CalibrationData v1;
  calDefaults(v1, 1);
  CHECK_NEAR(phFromVoltage(2.0f, v1.phV1, v1.phPh1, v1.phV2, v1.phPh2), 7.0, 1e-4);
  CHECK_NEAR(phFromVoltage(4.0f / 3.5f, v1.phV1, v1.phPh1, v1.phV2, v1.phPh2), 4.0, 1e-4);
  // Same result as the 2024 formula pH = 3.5 * V (tap water row of the alpha test).
  CHECK_NEAR(phFromVoltage(7.04f / 3.5f, v1.phV1, v1.phPh1, v1.phV2, v1.phPh2), 7.04, 1e-4);

  CalibrationData v2;
  calDefaults(v2, 2);
  CHECK_NEAR(phFromVoltage(1.5f, v2.phV1, v2.phPh1, v2.phV2, v2.phPh2), 7.0, 1e-4);
  CHECK_NEAR(phFromVoltage(2.03244f, v2.phV1, v2.phPh1, v2.phV2, v2.phPh2), 4.0, 1e-4);
  CHECK_NEAR(phFromVoltage(1.76622f, v2.phV1, v2.phPh1, v2.phV2, v2.phPh2), 5.5, 1e-3);  // higher V = more acidic

  CHECK(isNan(phFromVoltage(1.0f, 1.5f, 7.0f, 1.5f, 4.0f)));
  CHECK(isNan(phFromVoltage(notANumber(), 1.5f, 7.0f, 2.0f, 4.0f)));
}

static void testTds() {
  // k = 1, 25 C: identical to the original polynomial * 0.5.
  CHECK_NEAR(tdsFromVoltage(0.5f, 25.0f, 1.0f), 190.70375, 0.01);
  // Review regression: alpha-test raw median 34 at 23.94 C read 69 ppm.
  CHECK_NEAR(tdsFromVoltage(adc(34), 23.94f, 1.0f), 69.42, 0.05);
  // Review regression: Blynk showed 179.71 ppm (raw median 48, compensated as 0 C).
  // Correct compensation at 23.94 C gives about 96 ppm.
  CHECK_NEAR(tdsFromVoltage(adc(48), 23.94f, 1.0f), 96.35, 0.05);
  // Warmer water conducts better, so the same voltage means less TDS.
  CHECK(tdsFromVoltage(0.5f, 30.0f, 1.0f) < tdsFromVoltage(0.5f, 20.0f, 1.0f));
  CHECK(isNan(tdsFromVoltage(notANumber(), 25.0f, 1.0f)));

  const float k = tdsKValueFromStandard(0.9f, 18.0f, 707.0f);
  CHECK(!isNan(k));
  CHECK_NEAR(tdsFromVoltage(0.9f, 18.0f, k), 707.0, 0.5);
  CHECK(isNan(tdsKValueFromStandard(0.01f, 25.0f, 707.0f)));  // k would be far above 4
  CHECK(isNan(tdsKValueFromStandard(0.9f, 25.0f, -1.0f)));
}

static void testTurbidity() {
  CalibrationData c;
  calDefaults(c, 1);
  // Defaults reproduce the 2024 line NTU = (785.7 - ADC) / 1.22.
  CHECK_NEAR(turbidityFromVoltage(adc(608), c.turbVClear, c.turbVStd, c.turbNtuStd), 145.66, 0.01);
  CHECK_NEAR(turbidityFromVoltage(adc(252), c.turbVClear, c.turbVStd, c.turbNtuStd), 437.46, 0.01);
  CHECK_NEAR(turbidityFromVoltage(adc(900), c.turbVClear, c.turbVStd, c.turbNtuStd), 0.0, 1e-6);  // clamped
  CHECK_NEAR(turbidityFromVoltage(2.735f, 2.97f, 2.50f, 100.0f), 50.0, 1e-3);
  CHECK(isNan(turbidityFromVoltage(2.7f, 2.5f, 2.97f, 100.0f)));
}

static void testDissolvedOxygen() {
  CHECK_NEAR(doSaturationMgL(0.0f), 14.621, 0.005);
  CHECK_NEAR(doSaturationMgL(10.0f), 11.288, 0.005);
  CHECK_NEAR(doSaturationMgL(20.0f), 9.092, 0.005);
  CHECK_NEAR(doSaturationMgL(25.0f), 8.263, 0.005);
  CHECK_NEAR(doSaturationMgL(30.0f), 7.559, 0.005);

  CHECK_NEAR(doSaturationVoltage(25.0f, 1.6f, 25.0f, 0, 0, 1), 1.6, 1e-6);
  CHECK_NEAR(doSaturationVoltage(35.0f, 1.6f, 25.0f, 0, 0, 1), 1.95, 1e-5);
  CHECK_NEAR(doSaturationVoltage(20.0f, 1.6f, 25.0f, 1.2f, 15.0f, 2), 1.4, 1e-5);
  CHECK(isNan(doSaturationVoltage(20.0f, 1.6f, 25.0f, 0, 0, 0)));

  CHECK_NEAR(doFromVoltage(1.6f, 25.0f, 1.6f), doSaturationMgL(25.0f), 1e-5);  // 100 % saturation
  CHECK_NEAR(doFromVoltage(0.8f, 25.0f, 1.6f), doSaturationMgL(25.0f) / 2, 1e-5);
  CHECK(isNan(doFromVoltage(0.8f, 25.0f, 0.0f)));
}

static void testFilters() {
  uint16_t odd[] = {5, 1, 9, 3, 7};
  CHECK_NEAR(medianU16(odd, 5), 5.0, 1e-6);
  uint16_t even[] = {4, 1, 3, 2};
  CHECK_NEAR(medianU16(even, 4), 2.5, 1e-6);
  uint16_t spikes[] = {500, 501, 0, 499, 1023, 500};
  CHECK_NEAR(trimmedMeanU16(spikes, 6, 1), 500.0, 1e-6);
  uint16_t few[] = {10, 20};
  CHECK_NEAR(trimmedMeanU16(few, 2, 1), 15.0, 1e-6);
  uint16_t none[1] = {0};
  CHECK(isNan(medianU16(none, 0)));
  CHECK(isNan(trimmedMeanU16(none, 0, 1)));
}

static void testTextFormat() {
  char buf[16];
  CHECK_STR(formatFloat(buf, sizeof(buf), 179.712f, 2), "179.71");
  CHECK_STR(formatFloat(buf, sizeof(buf), 23.9375f, 1), "23.9");
  CHECK_STR(formatFloat(buf, sizeof(buf), 69.42f, 0), "69");
  CHECK_STR(formatFloat(buf, sizeof(buf), -0.0625f, 2), "-0.06");
  CHECK_STR(formatFloat(buf, sizeof(buf), -0.004f, 2), "0.00");
  CHECK_STR(formatFloat(buf, sizeof(buf), 0.5f, 0), "1");
  CHECK_STR(formatFloat(buf, sizeof(buf), notANumber(), 2), "nan");
  CHECK_STR(formatFloat(buf, sizeof(buf), 5.0e9f, 2), "ovf");
  char tiny[4];
  CHECK_STR(formatFloat(tiny, sizeof(tiny), 12345.0f, 0), "123");

  char msg[10] = "ab";
  appendText(msg, sizeof(msg), "cdef");
  CHECK_STR(msg, "abcdef");
  appendText(msg, sizeof(msg), "ghijk");
  CHECK_STR(msg, "abcdefghi");
}

static void testCalibrationStorage() {
  CalibrationData c;
  calDefaults(c, 1);
  CHECK(calIsValid(c));
  CHECK(c.flags == 0);
  c.tdsK = 1.5f;
  CHECK(!calIsValid(c));  // CRC no longer matches
  calSeal(c);
  CHECK(calIsValid(c));
  c.tdsK = notANumber();
  calSeal(c);
  CHECK(!calIsValid(c));
  calDefaults(c, 2);
  CHECK(calIsValid(c));
  c.magic = 0xFFFF;  // blank EEPROM
  calSeal(c);
  CHECK(!calIsValid(c));
}

static void testCalibrationPh() {
  CalibrationData c;
  calDefaults(c, 1);
  // One point: offset only, slope (3.5 pH/V) kept.
  CHECK(calApplyPhPoint(c, 1, 2.10f, 7.0f));
  CHECK(!calPhDone(c));
  CHECK_NEAR(phFromVoltage(2.10f, c.phV1, c.phPh1, c.phV2, c.phPh2), 7.0, 1e-4);
  CHECK_NEAR(phFromVoltage(2.10f - 3.0f / 3.5f, c.phV1, c.phPh1, c.phV2, c.phPh2), 4.0, 1e-4);
  // Second point: true two-point line through (2.10, 7) and (1.30, 4).
  CHECK(calApplyPhPoint(c, 2, 1.30f, 4.0f));
  CHECK(calPhDone(c));
  CHECK_NEAR(phFromVoltage(1.70f, c.phV1, c.phPh1, c.phV2, c.phPh2), 5.5, 1e-4);
  CHECK(calIsValid(c));
  // Rejected: buffers too close, voltage out of range, bad point index.
  CalibrationData before = c;
  CHECK(!calApplyPhPoint(c, 2, 1.90f, 6.5f));
  CHECK(!calApplyPhPoint(c, 1, 5.0f, 7.0f));
  CHECK(!calApplyPhPoint(c, 3, 1.5f, 7.0f));
  CHECK(memcmp(&before, &c, sizeof(c)) == 0);

  CalibrationData v2;
  calDefaults(v2, 2);
  CHECK(calApplyPhPoint(v2, 1, 1.52f, 7.0f));  // V2 keeps its negative slope
  CHECK(phFromVoltage(1.80f, v2.phV1, v2.phPh1, v2.phV2, v2.phPh2) < 7.0f);
}

static void testCalibrationOthers() {
  CalibrationData c;
  calDefaults(c, 1);

  CHECK(calApplyTds(c, 0.9f, 18.0f, 707.0f));
  CHECK(calTdsDone(c));
  CHECK_NEAR(tdsFromVoltage(0.9f, 18.0f, c.tdsK), 707.0, 0.5);
  CHECK(!calApplyTds(c, 0.01f, 18.0f, 707.0f));
  CHECK(!calApplyTds(c, 0.9f, 45.0f, 707.0f));

  // Turbidity: zero first keeps the default slope, then the standard sets it.
  const float slope = c.turbNtuStd / (c.turbVClear - c.turbVStd);
  CHECK(calApplyTurbidityZero(c, 2.97f));
  CHECK_NEAR(turbidityFromVoltage(2.97f, c.turbVClear, c.turbVStd, c.turbNtuStd), 0.0, 1e-4);
  CHECK_NEAR(turbidityFromVoltage(2.87f, c.turbVClear, c.turbVStd, c.turbNtuStd), 0.1 * slope, 0.01);
  CHECK(!calTurbidityDone(c));
  CHECK(calApplyTurbidityStandard(c, 2.50f, 100.0f));
  CHECK(calTurbidityDone(c));
  CHECK_NEAR(turbidityFromVoltage(2.735f, c.turbVClear, c.turbVStd, c.turbNtuStd), 50.0, 1e-3);
  CHECK(!calApplyTurbidityStandard(c, 3.0f, 100.0f));  // standard must read below clear water
  CHECK(!calApplyTurbidityZero(c, 2.49f));             // clear water must read above the standard

  CHECK(calApplyTempReference(c, 23.5f, 24.0f));
  CHECK_NEAR(c.tempOffsetC, 0.5, 1e-6);
  CHECK(!calApplyTempReference(c, 23.5f, 30.0f));

  CHECK(calApplyVcc(c, 225.0f, 5.0f));
  CHECK_NEAR(c.bandgapV, 5.0 * 225.0 / 1024.0, 1e-5);
  CHECK(calVccDone(c));
  CHECK(!calApplyVcc(c, 300.0f, 5.0f));  // would imply a 1.46 V reference

  CHECK(!calApplyDoPoint(c, 2, 1.2f, 15.0f));  // point 2 needs point 1
  CHECK(calApplyDoPoint(c, 1, 1.6f, 25.0f));
  CHECK(calDoPoints(c) == 1);
  CHECK(!calApplyDoPoint(c, 2, 1.5f, 22.0f));  // temperatures too close
  CHECK(calApplyDoPoint(c, 2, 1.2f, 15.0f));
  CHECK(calDoPoints(c) == 2);
  CHECK(calIsValid(c));
}

static void testCommands() {
  CHECK(parseCommand("help").type == CMD_HELP);
  CHECK(parseCommand("?").type == CMD_HELP);
  CHECK(parseCommand("STATUS").type == CMD_STATUS);
  CHECK(parseCommand("q").type == CMD_EXIT);
  CHECK(parseCommand("").type == CMD_NONE);
  CHECK(parseCommand("   \r\n").type == CMD_NONE);

  Command c = parseCommand("  cal   PH1   7.00 \r");
  CHECK(c.type == CMD_CAL_PH && c.point == 1);
  CHECK_NEAR(c.value, 7.0, 1e-6);
  c = parseCommand("cal ph2 4");
  CHECK(c.type == CMD_CAL_PH && c.point == 2);
  CHECK_NEAR(c.value, 4.0, 1e-6);
  c = parseCommand("cal tds 707");
  CHECK(c.type == CMD_CAL_TDS);
  CHECK_NEAR(c.value, 707.0, 1e-6);
  CHECK(parseCommand("cal ntu0").type == CMD_CAL_NTU_ZERO);
  CHECK(parseCommand("cal ntu 0").type == CMD_CAL_NTU_ZERO);
  c = parseCommand("cal ntu 100");
  CHECK(c.type == CMD_CAL_NTU);
  CHECK_NEAR(c.value, 100.0, 1e-6);
  c = parseCommand("cal temp -1.5");
  CHECK(c.type == CMD_CAL_TEMP);
  CHECK_NEAR(c.value, -1.5, 1e-6);
  CHECK(parseCommand("cal vcc 4.98").type == CMD_CAL_VCC);
  c = parseCommand("cal do2");
  CHECK(c.type == CMD_CAL_DO && c.point == 2);
  CHECK(parseCommand("cal reset").type == CMD_CAL_RESET);

  CHECK(parseCommand("cal ph1").type == CMD_INVALID);
  CHECK(parseCommand("cal ph1 abc").type == CMD_INVALID);
  CHECK(parseCommand("cal ph1 7 8").type == CMD_INVALID);
  CHECK(parseCommand("cal").type == CMD_INVALID);
  CHECK(parseCommand("cal xyz 1").type == CMD_INVALID);
  CHECK(parseCommand("cal reset now").type == CMD_INVALID);
  CHECK(parseCommand("format c:").type == CMD_INVALID);
  CHECK(parseCommand("cal ph1 7.000000000000000000000000000000000000000000001").type == CMD_INVALID);
}

static void testAerator() {
  AeratorConfig cfg = {5.0f, 7.0f, 900};
  AeratorController a(cfg);
  a.begin(0);
  CHECK(a.isOn());
  CHECK(a.update(8.0f, 100));    // minimum run time not reached
  CHECK(!a.update(8.0f, 1000));  // high DO -> off
  CHECK(!a.update(6.0f, 1100));  // inside hysteresis band -> stays off
  CHECK(a.update(4.9f, 1200));   // low DO -> on immediately
  CHECK(a.update(7.5f, 1300));   // just switched on -> keeps running
  CHECK(!a.update(7.5f, 2200));
  CHECK(a.update(notANumber(), 2300));  // unknown DO -> fail-safe on
}

static SensorPacket packet(uint16_t seq) {
  SensorPacket p;
  memset(&p, 0, sizeof(p));
  p.version = PROTOCOL_VERSION;
  p.nodeId = 1;
  p.seq = seq;
  p.vccMillivolts = 5000;
  p.temperatureC = 22.0f;
  p.pH = 7.2f;
  p.turbidityNtu = 12.0f;
  p.tdsPpm = 150.0f;
  p.dissolvedOxygenMgL = 8.0f;
  p.aeratorOn = 1;
  return p;
}

static void testGatewayPackets() {
  GatewayLogic g(180000UL, defaultAlertThresholds());
  SensorPacket out;
  SensorPacket p = packet(0);
  CHECK(g.accept(&p, PACKET_SIZE - 1, 0, out) == PACKET_BAD_SIZE);
  p.version = 1;
  CHECK(g.accept(&p, PACKET_SIZE, 0, out) == PACKET_BAD_VERSION);
  CHECK(!g.online());

  const uint16_t seqs[] = {0, 1, 2, 5};
  for (uint8_t i = 0; i < 4; ++i) {
    p = packet(seqs[i]);
    CHECK(g.accept(&p, PACKET_SIZE, 1000UL * i, out) == PACKET_OK);
  }
  CHECK(g.received() == 4);
  CHECK(g.lost() == 2);
  CHECK_NEAR(g.lossPercent(), 100.0 * 2 / 6, 1e-4);
  CHECK(out.seq == 5);

  CHECK(g.accept(&p, PACKET_SIZE, 4000, out) == PACKET_DUPLICATE);
  CHECK(g.received() == 4);
  p = packet(0);  // node restart
  CHECK(g.accept(&p, PACKET_SIZE, 5000, out) == PACKET_OK);
  CHECK(g.lost() == 2);
  p = packet(3000);  // implausible jump: resynchronise without counting loss
  CHECK(g.accept(&p, PACKET_SIZE, 6000, out) == PACKET_OK);
  CHECK(g.lost() == 2);

  CHECK(g.updateOnline(6000) == 1);
  CHECK(g.online());
  CHECK(g.updateOnline(100000) == 0);
  CHECK(g.updateOnline(6000 + 180001UL) == -1);
  CHECK(!g.online());
  p = packet(3001);
  g.accept(&p, PACKET_SIZE, 200000UL, out);
  CHECK(g.updateOnline(200000UL) == 1);
}

static void testGatewayAlerts() {
  AlertThresholds t = defaultAlertThresholds();
  t.cooldownMs = 1000;
  GatewayLogic g(180000UL, t);
  char msg[64];

  SensorPacket p = packet(1);
  CHECK(g.checkAlerts(p, 0, msg, sizeof(msg)) == 0);
  CHECK_STR(msg, "");

  p.pH = 9.5f;
  CHECK(g.checkAlerts(p, 10, msg, sizeof(msg)) == ALERT_PH);
  CHECK_STR(msg, "pH 9.50");
  CHECK(g.checkAlerts(p, 500, msg, sizeof(msg)) == 0);  // cooldown
  CHECK(g.checkAlerts(p, 1010, msg, sizeof(msg)) == ALERT_PH);

  p.status = ST_PH_UNCALIBRATED;  // uncalibrated values never alert
  CHECK(g.checkAlerts(p, 5000, msg, sizeof(msg)) == 0);

  p = packet(2);
  p.dissolvedOxygenMgL = 3.2f;
  p.temperatureC = 31.0f;
  CHECK(g.checkAlerts(p, 6000, msg, sizeof(msg)) == (ALERT_TEMP | ALERT_DO));
  CHECK_STR(msg, "Temp 31.0 C; DO 3.2 mg/L");

  p = packet(3);
  p.vccMillivolts = 4300;
  p.status = ST_VCC_UNCALIBRATED;
  CHECK(g.checkAlerts(p, 7000, msg, sizeof(msg)) == 0);
  p.status = 0;
  CHECK(g.checkAlerts(p, 7000, msg, sizeof(msg)) == ALERT_VCC);
  CHECK_STR(msg, "Node supply 4.30 V");

  p = packet(4);
  p.tdsPpm = notANumber();
  p.status = ST_TDS_INVALID;
  p.turbidityNtu = 250.0f;
  CHECK(g.checkAlerts(p, 8000, msg, sizeof(msg)) == ALERT_TURBIDITY);
}

// ---- GatewayApp with mock radio / Blynk / serial --------------------------------

struct MockRadio {
  SensorPacket queue[8];
  int head = 0;
  int tail = 0;
  bool available() { return head != tail; }
  void read(void* buf, uint8_t len) { memcpy(buf, &queue[head++ % 8], len); }
  void push(const SensorPacket& p) { queue[tail++ % 8] = p; }
};

struct MockBlynk {
  double last[16];
  int writes[16];
  std::string events;
  MockBlynk() {
    for (int i = 0; i < 16; ++i) {
      last[i] = -1;
      writes[i] = 0;
    }
  }
  template <typename T>
  void virtualWrite(int pin, T value) {
    last[pin] = static_cast<double>(value);
    ++writes[pin];
  }
  template <typename N>
  void logEvent(const N& name) {
    events += std::string(name) + ";";
  }
  template <typename N, typename D>
  void logEvent(const N& name, const D& description) {
    events += std::string(name) + "|" + description + ";";
  }
};

struct MockLog {
  std::string out;
  void print(const char* s) { out += s; }
  void print(char c) { out += c; }
  void print(unsigned long v) { out += std::to_string(v); }
  void print(unsigned int v, int base = 10) {
    char b[16];
    snprintf(b, sizeof(b), base == 16 ? "%X" : "%u", v);
    out += b;
  }
  void print(float v, int digits) {
    char b[32];
    snprintf(b, sizeof(b), "%.*f", digits, static_cast<double>(v));
    out += b;
  }
  void println(const char* s) { out += std::string(s) + "\n"; }
  void println(float v, int digits) {
    print(v, digits);
    out += "\n";
  }
};

static void testGatewayApp() {
  MockRadio radio;
  MockBlynk blynk;
  MockLog log;
  AlertThresholds t = defaultAlertThresholds();
  GatewayApp<MockRadio, MockBlynk, MockLog> app(radio, blynk, log, 300000UL, t);

  SensorPacket p = packet(0);
  p.dissolvedOxygenMgL = notANumber();  // no DO sensor
  p.aeratorOn = AERATOR_UNCONTROLLED;
  p.status = ST_DO_INVALID;
  radio.push(p);
  p = packet(1);
  p.pH = 9.5f;
  radio.push(p);
  app.pollRadio(1000);
  CHECK(!radio.available());  // FIFO drained in one call
  CHECK(blynk.writes[VPIN_TEMPERATURE] == 2 && blynk.writes[VPIN_PH] == 2 && blynk.writes[VPIN_TDS] == 2);
  CHECK(blynk.writes[VPIN_DO] == 1);       // NaN from the first packet was not sent
  CHECK(blynk.writes[VPIN_AERATOR] == 1);  // uncontrolled aerator not sent
  CHECK_NEAR(blynk.last[VPIN_PH], 9.5, 1e-6);
  CHECK_NEAR(blynk.last[VPIN_NODE_VCC], 5.0, 1e-6);
  CHECK(blynk.events == "water_quality_alert|pH 9.50;");
  CHECK(log.out.find("CSV,1000,1,22.00,9.50,12.0,150,8.00,1,5.000,0x0,0.0") != std::string::npos);

  app.checkNode(2000);
  CHECK(blynk.last[VPIN_NODE_ONLINE] == 1);
  CHECK(blynk.events.find("node_online;") != std::string::npos);
  app.checkNode(1000 + 300001UL);
  CHECK(blynk.last[VPIN_NODE_ONLINE] == 0);
  CHECK(blynk.events.find("node_offline;") != std::string::npos);

  p = packet(2);
  p.version = 1;
  radio.push(p);
  app.pollRadio(400000UL);
  CHECK(log.out.find("other protocol version") != std::string::npos);
  CHECK(blynk.writes[VPIN_TEMPERATURE] == 2);
}

int main() {
  testProtocol();
  testPh();
  testTds();
  testTurbidity();
  testDissolvedOxygen();
  testFilters();
  testTextFormat();
  testCalibrationStorage();
  testCalibrationPh();
  testCalibrationOthers();
  testCommands();
  testAerator();
  testGatewayPackets();
  testGatewayAlerts();
  testGatewayApp();
  printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
