// Pond node (Arduino Uno): measures water temperature, pH, TDS, turbidity and
// (optionally) dissolved oxygen, then sends one packet per interval to the
// gateway over nRF24L01.
//
// Wiring: docs/hardware.md    Calibration: docs/calibration.md
// Libraries: RF24 1.6.2, OneWire 2.3.8, DallasTemperature 4.0.6, PondCore (this repository)
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <PondCore.h>
#include <RF24.h>
#include <SPI.h>

#include "config.h"
#include "platform.h"

#if ENABLE_AERATOR_CONTROL && !ENABLE_DO_SENSOR
#error "ENABLE_AERATOR_CONTROL needs ENABLE_DO_SENSOR: the aerator is switched on dissolved oxygen"
#endif

using namespace pond;

static const char FIRMWARE_VERSION[] = "2.0.0";
static const uint8_t MAX_SAMPLES = 40;
static_assert(PH_SAMPLES <= MAX_SAMPLES && TDS_SAMPLES <= MAX_SAMPLES && TURB_SAMPLES <= MAX_SAMPLES &&
                  DO_SAMPLES <= MAX_SAMPLES,
              "sample counts must fit the sample buffer");

struct Reading {
  float vcc;            // supply voltage used for all conversions
  float bandgapCounts;  // raw internal-reference reading (for "cal vcc")
  float phVolts, tdsVolts, turbVolts, doVolts;
  float tempRawC;       // DS18B20 without offset, NaN if invalid
  float tempC;
  float pH, tdsPpm, turbidityNtu, doMgL;
  uint16_t status;      // StatusFlag bits
};

RF24 radio(PIN_RF_CE, PIN_RF_CSN);
OneWire oneWire(PIN_DS18B20);
DallasTemperature thermometer(&oneWire);
DeviceAddress thermometerAddress;
bool thermometerFound = false;
uint32_t conversionStartMs = 0;

CalibrationData cal;
bool calibrationFromDefaults = false;

const AeratorConfig aeratorConfig = {DO_LOW_MGL, DO_HIGH_MGL, AERATOR_MIN_ON_S};
AeratorController aerator(aeratorConfig);

bool radioReady = false;
bool previousTxFailed = false;
uint16_t sequence = 0;
uint32_t sleptMs = 0;  // time spent in power-down; millis() does not count it

uint16_t samples[MAX_SAMPLES];
char lineBuffer[48];
uint8_t lineLength = 0;
bool lineOverflow = false;

uint32_t uptimeMs() { return millis() + sleptMs; }

bool inRange(float x, float low, float high) { return !isNan(x) && x >= low && x <= high; }

// ---- Calibration storage ----------------------------------------------------

void saveCalibration() {
  calSeal(cal);
  EEPROM.put(CAL_EEPROM_ADDRESS, cal);  // put() only rewrites changed bytes
  calibrationFromDefaults = false;
}

void loadCalibration() {
  EEPROM.get(CAL_EEPROM_ADDRESS, cal);
  if (!calIsValid(cal)) {
    calDefaults(cal, PH_BOARD_VERSION);
    calibrationFromDefaults = true;
    return;
  }
  // PH_BOARD_VERSION changed and pH never calibrated: switch to that board's default line.
  if (cal.phBoardVersion != PH_BOARD_VERSION && !(cal.flags & (CAL_PH_P1 | CAL_PH_P2))) {
    CalibrationData d;
    calDefaults(d, PH_BOARD_VERSION);
    cal.phBoardVersion = d.phBoardVersion;
    cal.phV1 = d.phV1;
    cal.phPh1 = d.phPh1;
    cal.phV2 = d.phV2;
    cal.phPh2 = d.phPh2;
    calSeal(cal);
  }
}

// ---- Radio ------------------------------------------------------------------

bool setupRadio() {
  if (!radio.begin() || !radio.isChipConnected()) return false;
  radio.setChannel(POND_RF_CHANNEL);
  if (!radio.setDataRate(POND_RF_DATA_RATE)) Serial.println(F("# radio: data rate not supported by this chip"));
  radio.setPALevel(POND_RF_PA_LEVEL);
  radio.setPayloadSize(PACKET_SIZE);
  radio.setAutoAck(true);
  radio.setRetries(RF_RETRY_DELAY, RF_RETRY_COUNT);
  radio.openWritingPipe(reinterpret_cast<const uint8_t*>(POND_RF_ADDRESS));
  radio.stopListening();
  radio.powerDown();
  return true;
}

bool transmit(const SensorPacket& packet) {
  if (!radioReady) radioReady = setupRadio();
  if (!radioReady) return false;
  radio.powerUp();
  const bool ok = radio.write(&packet, PACKET_SIZE);  // waits for the gateway's ACK (auto-retry)
  radio.powerDown();
  if (!ok && !radio.isChipConnected()) radioReady = false;  // re-initialise next time
  return ok;
}

// ---- Sensors ----------------------------------------------------------------

void setTdsPower(bool on) {
#if PIN_TDS_POWER >= 0
  digitalWrite(PIN_TDS_POWER, on ? HIGH : LOW);
#else
  (void)on;
#endif
}

float sampleVolts(uint8_t pin, uint8_t count, uint16_t intervalMs, float vcc) {
  analogRead(pin);  // discard the first conversion after switching the multiplexer
  for (uint8_t i = 0; i < count; ++i) {
    samples[i] = analogRead(pin);
    if (intervalMs > 0) delay(intervalMs);
  }
  return adcToVolts(trimmedMeanU16(samples, count, count / 10), vcc);
}

void findThermometer() {
  thermometer.begin();
  thermometerFound = thermometer.getAddress(thermometerAddress, 0);
  if (thermometerFound) thermometer.setResolution(thermometerAddress, TEMP_RESOLUTION_BITS);
}

void startTemperatureConversion() {
  if (!thermometerFound) findThermometer();
  if (thermometerFound) thermometer.requestTemperaturesByAddress(thermometerAddress);
  conversionStartMs = millis();
}

float readTemperatureRawC() {
  if (!thermometerFound) return notANumber();
  const uint16_t waitMs = DallasTemperature::millisToWaitForConversion(TEMP_RESOLUTION_BITS);
  while (millis() - conversionStartMs < waitMs) {
  }
  const float t = thermometer.getTempC(thermometerAddress, 1);  // scratchpad CRC is checked
  if (t == DEVICE_DISCONNECTED_C) {
    thermometerFound = false;  // search the bus again next time
    return notANumber();
  }
  if (t == 85.0f || t < TEMP_MIN_VALID_C || t > TEMP_MAX_VALID_C) return notANumber();  // 85 C = power-on value
  return t;
}

float supplyVoltage(float& bandgapCounts) {
  bandgapCounts = platformReadBandgapCounts();
  if (calVccDone(cal) && bandgapCounts > 0.0f) return cal.bandgapV * 1024.0f / bandgapCounts;
  return NOMINAL_VCC;
}

void measure(Reading& r) {
  r.status = 0;
  startTemperatureConversion();  // runs in the background while the analog sensors are sampled
  r.vcc = supplyVoltage(r.bandgapCounts);

  setTdsPower(false);  // pH, turbidity and DO are sampled without the TDS excitation (if switchable)
  r.phVolts = sampleVolts(PIN_PH, PH_SAMPLES, PH_SAMPLE_INTERVAL_MS, r.vcc);
  r.turbVolts = sampleVolts(PIN_TURBIDITY, TURB_SAMPLES, TURB_SAMPLE_INTERVAL_MS, r.vcc);
#if ENABLE_DO_SENSOR
  r.doVolts = sampleVolts(PIN_DO, DO_SAMPLES, DO_SAMPLE_INTERVAL_MS, r.vcc);
#else
  r.doVolts = notANumber();
#endif
#if PIN_TDS_POWER >= 0
  setTdsPower(true);
  delay(TDS_WARMUP_MS);
#endif
  r.tdsVolts = sampleVolts(PIN_TDS, TDS_SAMPLES, TDS_SAMPLE_INTERVAL_MS, r.vcc);
  setTdsPower(false);
  r.tempRawC = readTemperatureRawC();

  const bool tempValid = !isNan(r.tempRawC);
  r.tempC = tempValid ? r.tempRawC + cal.tempOffsetC : notANumber();
  if (!tempValid) r.status |= ST_TEMP_INVALID | ST_TEMP_FALLBACK;
  const float compensationC = tempValid ? r.tempC : 25.0f;

  r.pH = inRange(r.phVolts, PH_VOLTS_MIN, PH_VOLTS_MAX)
             ? phFromVoltage(r.phVolts, cal.phV1, cal.phPh1, cal.phV2, cal.phPh2)
             : notANumber();
  if (!inRange(r.pH, 0.0f, 14.0f)) {
    r.pH = notANumber();
    r.status |= ST_PH_INVALID;
  }
  if (!calPhDone(cal)) r.status |= ST_PH_UNCALIBRATED;

  r.tdsPpm = inRange(r.tdsVolts, 0.0f, TDS_VOLTS_MAX) ? tdsFromVoltage(r.tdsVolts, compensationC, cal.tdsK)
                                                       : notANumber();
  if (isNan(r.tdsPpm)) r.status |= ST_TDS_INVALID;
  if (!calTdsDone(cal)) r.status |= ST_TDS_UNCALIBRATED;

  r.turbidityNtu = inRange(r.turbVolts, TURB_VOLTS_MIN, TURB_VOLTS_MAX)
                       ? turbidityFromVoltage(r.turbVolts, cal.turbVClear, cal.turbVStd, cal.turbNtuStd)
                       : notANumber();
  if (isNan(r.turbidityNtu)) r.status |= ST_TURB_INVALID;
  if (!calTurbidityDone(cal)) r.status |= ST_TURB_UNCALIBRATED;

#if ENABLE_DO_SENSOR
  const float vSat = doSaturationVoltage(compensationC, cal.doV1, cal.doT1, cal.doV2, cal.doT2, calDoPoints(cal));
  r.doMgL = inRange(r.doVolts, 0.0f, DO_VOLTS_MAX) ? doFromVoltage(r.doVolts, compensationC, vSat) : notANumber();
  if (!inRange(r.doMgL, 0.0f, DO_MAX_VALID_MGL)) {
    r.doMgL = notANumber();
    r.status |= ST_DO_INVALID;
  }
  if (!calDoDone(cal)) r.status |= ST_DO_UNCALIBRATED;
#else
  r.doMgL = notANumber();
  r.status |= ST_DO_INVALID;
#endif
  if (!calVccDone(cal)) r.status |= ST_VCC_UNCALIBRATED;
  if (calibrationFromDefaults) r.status |= ST_CAL_DEFAULTS;
}

// ---- Aerator ----------------------------------------------------------------

void setAeratorRelay(bool aeratorOn) {
#if ENABLE_AERATOR_CONTROL
  const bool energize = AERATOR_ON_WHEN_RELAY_ENERGIZED ? aeratorOn : !aeratorOn;
  const bool levelHigh = RELAY_ACTIVE_LOW ? !energize : energize;
  digitalWrite(PIN_RELAY, levelHigh ? HIGH : LOW);
#else
  (void)aeratorOn;
#endif
}

// ---- Output -----------------------------------------------------------------

SensorPacket buildPacket(const Reading& r) {
  SensorPacket p;
  p.version = PROTOCOL_VERSION;
  p.nodeId = NODE_ID;
  p.seq = sequence;
  p.status = r.status;
  if (previousTxFailed) p.status |= ST_PREV_TX_FAILED;
#if ENABLE_AERATOR_CONTROL
  p.status |= ST_AERATOR_CONTROL;
  p.aeratorOn = aerator.isOn() ? 1 : 0;
#else
  p.aeratorOn = AERATOR_UNCONTROLLED;
#endif
  p.vccMillivolts = static_cast<uint16_t>(r.vcc * 1000.0f + 0.5f);
  p.temperatureC = r.tempC;
  p.pH = r.pH;
  p.turbidityNtu = r.turbidityNtu;
  p.tdsPpm = r.tdsPpm;
  p.dissolvedOxygenMgL = r.doMgL;
  return p;
}

void printCsvHeader() {
  Serial.println(F("# seq,temp_c,ph,turbidity_ntu,tds_ppm,do_mgl,aerator,vcc_v,status_hex,tx"));
}

void printCsv(const SensorPacket& p, bool txOk) {
  Serial.print(p.seq);
  Serial.print(',');
  Serial.print(p.temperatureC, 2);
  Serial.print(',');
  Serial.print(p.pH, 2);
  Serial.print(',');
  Serial.print(p.turbidityNtu, 1);
  Serial.print(',');
  Serial.print(p.tdsPpm, 0);
  Serial.print(',');
  Serial.print(p.dissolvedOxygenMgL, 2);
  Serial.print(',');
  Serial.print(p.aeratorOn);
  Serial.print(',');
  Serial.print(p.vccMillivolts / 1000.0f, 3);
  Serial.print(F(",0x"));
  Serial.print(p.status, HEX);
  Serial.println(txOk ? F(",ok") : F(",fail"));
}

void printReading(const Reading& r) {
  Serial.print(F("T "));
  Serial.print(r.tempC, 2);
  Serial.print(F(" C | pH "));
  Serial.print(r.pH, 2);
  Serial.print(F(" ("));
  Serial.print(r.phVolts, 3);
  Serial.print(F(" V) | TDS "));
  Serial.print(r.tdsPpm, 0);
  Serial.print(F(" ppm ("));
  Serial.print(r.tdsVolts, 3);
  Serial.print(F(" V) | NTU "));
  Serial.print(r.turbidityNtu, 1);
  Serial.print(F(" ("));
  Serial.print(r.turbVolts, 3);
  Serial.print(F(" V)"));
#if ENABLE_DO_SENSOR
  Serial.print(F(" | DO "));
  Serial.print(r.doMgL, 2);
  Serial.print(F(" mg/L ("));
  Serial.print(r.doVolts, 3);
  Serial.print(F(" V)"));
#endif
  Serial.print(F(" | Vcc "));
  Serial.print(r.vcc, 3);
  Serial.print(F(" V | status 0x"));
  Serial.println(r.status, HEX);
}

void printCalibration() {
  Serial.print(F("cal flags 0x"));
  Serial.print(cal.flags, HEX);
  Serial.print(F(" | pH board V"));
  Serial.print(cal.phBoardVersion);
  Serial.print(F(" ("));
  Serial.print(cal.phV1, 3);
  Serial.print(F(" V = pH "));
  Serial.print(cal.phPh1, 2);
  Serial.print(F(", "));
  Serial.print(cal.phV2, 3);
  Serial.print(F(" V = pH "));
  Serial.print(cal.phPh2, 2);
  Serial.print(F(") | TDS k "));
  Serial.print(cal.tdsK, 3);
  Serial.print(F(" | NTU 0 @ "));
  Serial.print(cal.turbVClear, 3);
  Serial.print(F(" V, "));
  Serial.print(cal.turbNtuStd, 1);
  Serial.print(F(" @ "));
  Serial.print(cal.turbVStd, 3);
  Serial.print(F(" V | temp offset "));
  Serial.print(cal.tempOffsetC, 2);
  Serial.print(F(" C | bandgap "));
  Serial.print(cal.bandgapV, 4);
  Serial.print(F(" V"));
#if ENABLE_DO_SENSOR
  Serial.print(F(" | DO sat "));
  Serial.print(cal.doV1, 3);
  Serial.print(F(" V @ "));
  Serial.print(cal.doT1, 1);
  Serial.print(F(" C"));
  if (calDoPoints(cal) == 2) {
    Serial.print(F(", "));
    Serial.print(cal.doV2, 3);
    Serial.print(F(" V @ "));
    Serial.print(cal.doT2, 1);
    Serial.print(F(" C"));
  }
#endif
  Serial.println();
}

void printHelp() {
  Serial.println(F("Commands (finish with Enter):"));
  Serial.println(F("  status | s           measure now and show raw voltages + calibration"));
  Serial.println(F("  cal vcc <V>          multimeter reading of the 5V pin (do this first)"));
  Serial.println(F("  cal temp <C>         reference thermometer reading"));
  Serial.println(F("  cal ph1 <pH>         probe in buffer 1, e.g. cal ph1 7.00"));
  Serial.println(F("  cal ph2 <pH>         probe in buffer 2, e.g. cal ph2 4.00"));
  Serial.println(F("  cal tds <ppm>        probe in standard, e.g. cal tds 707"));
  Serial.println(F("  cal ntu0             turbidity sensor in clear (distilled) water"));
  Serial.println(F("  cal ntu <NTU>        turbidity sensor in a standard, e.g. cal ntu 100"));
#if ENABLE_DO_SENSOR
  Serial.println(F("  cal do1 | cal do2    DO probe in air-saturated water (2nd point: other temp.)"));
#endif
  Serial.println(F("  cal reset            restore defaults"));
  Serial.println(F("  exit | q             leave maintenance mode"));
}

// ---- Maintenance mode -------------------------------------------------------

// True when a complete line is in lineBuffer.
bool readSerialLine() {
  while (Serial.available() > 0) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\n' || ch == '\r') {
      if (lineLength == 0 && !lineOverflow) continue;
      lineBuffer[lineLength] = '\0';
      if (lineOverflow) strcpy(lineBuffer, "!");  // reject overlong input as a whole
      lineLength = 0;
      lineOverflow = false;
      return true;
    }
    if (lineLength < sizeof(lineBuffer) - 1) {
      lineBuffer[lineLength++] = ch;
    } else {
      lineOverflow = true;
    }
  }
  return false;
}

// Returns false when maintenance mode should end.
bool handleCommand(const char* line) {
  const Command cmd = parseCommand(line);
  switch (cmd.type) {
    case CMD_NONE:
      return true;
    case CMD_EXIT:
      return false;
    case CMD_HELP:
      printHelp();
      return true;
    case CMD_INVALID:
      Serial.println(F("? unknown command - type help"));
      return true;
    case CMD_CAL_RESET:
      calDefaults(cal, PH_BOARD_VERSION);
      saveCalibration();
      Serial.println(F("OK, defaults restored and saved"));
      printCalibration();
      return true;
    default:
      break;
  }

  Reading r;
  measure(r);  // fresh reading for status and calibration
  bool ok = false;
  switch (cmd.type) {
    case CMD_STATUS:
      printReading(r);
      printCalibration();
      return true;
    case CMD_CAL_PH:
      ok = calApplyPhPoint(cal, cmd.point, r.phVolts, cmd.value);
      break;
    case CMD_CAL_TDS:
      ok = !isNan(r.tempC) && calApplyTds(cal, r.tdsVolts, r.tempC, cmd.value);
      break;
    case CMD_CAL_NTU_ZERO:
      ok = calApplyTurbidityZero(cal, r.turbVolts);
      break;
    case CMD_CAL_NTU:
      ok = calApplyTurbidityStandard(cal, r.turbVolts, cmd.value);
      break;
    case CMD_CAL_TEMP:
      ok = calApplyTempReference(cal, r.tempRawC, cmd.value);
      break;
    case CMD_CAL_VCC:
      ok = calApplyVcc(cal, r.bandgapCounts, cmd.value);
      break;
    case CMD_CAL_DO:
      ok = ENABLE_DO_SENSOR && !isNan(r.tempC) && calApplyDoPoint(cal, cmd.point, r.doVolts, r.tempC);
      break;
    default:
      break;
  }
  if (ok) {
    saveCalibration();
    Serial.println(F("OK, saved to EEPROM"));
    printCalibration();
  } else {
    Serial.println(F("REJECTED: reading out of range, temperature missing, or too close to the other point"));
    printReading(r);
  }
  return true;
}

void maintenanceMode() {
  Serial.println(F("== Maintenance mode: radio and sleep are off, nothing is sent to the dashboard =="));
  printHelp();
  uint32_t lastPrintMs = millis();
  for (;;) {
    if (readSerialLine()) {
      if (!handleCommand(lineBuffer)) break;
      lastPrintMs = millis();
    }
    if (millis() - lastPrintMs >= 3000UL) {
      Reading r;
      measure(r);
      printReading(r);
      lastPrintMs = millis();
    }
  }
  Serial.println(F("== Maintenance mode finished =="));
  printCsvHeader();
}

void waitUntilNextCycle(uint32_t ms) {
#if ENABLE_SLEEP
  Serial.flush();
  sleptMs += platformSleep(ms);
#else
  const uint32_t start = millis();
  while (millis() - start < ms) {
    if (readSerialLine()) {  // any command opens maintenance mode
      if (handleCommand(lineBuffer)) maintenanceMode();
      return;
    }
  }
#endif
}

// ---- Arduino entry points -----------------------------------------------------

void setup() {
  platformInit();
#if ENABLE_AERATOR_CONTROL
  setAeratorRelay(true);  // level first, then output: no relay glitch at boot
  pinMode(PIN_RELAY, OUTPUT);
#endif
#if PIN_TDS_POWER >= 0
  digitalWrite(PIN_TDS_POWER, LOW);
  pinMode(PIN_TDS_POWER, OUTPUT);
#endif
  Serial.begin(SERIAL_BAUD);
  Serial.print(F("Pond node "));
  Serial.print(FIRMWARE_VERSION);
  Serial.print(F(", protocol "));
  Serial.print(PROTOCOL_VERSION);
  Serial.print(F(", node "));
  Serial.println(NODE_ID);

  loadCalibration();
  if (calibrationFromDefaults) Serial.println(F("# no stored calibration, using defaults (see docs/calibration.md)"));

  thermometer.setWaitForConversion(false);
  findThermometer();
  if (!thermometerFound) Serial.println(F("# DS18B20 not found"));

  radioReady = setupRadio();
  if (!radioReady) Serial.println(F("# nRF24L01 not responding: check wiring and the 3.3 V adapter"));

#if ENABLE_AERATOR_CONTROL
  aerator.begin(uptimeMs() / 1000UL);
#endif

  Serial.print(F("Press Enter within "));
  Serial.print(MAINTENANCE_WINDOW_MS / 1000UL);
  Serial.println(F(" s for maintenance/calibration mode..."));
  const uint32_t start = millis();
  while (millis() - start < MAINTENANCE_WINDOW_MS) {
    if (Serial.available() > 0) {
      while (Serial.available() > 0) Serial.read();
      maintenanceMode();
      return;
    }
  }
  printCsvHeader();
}

void loop() {
  const uint32_t cycleStartMs = uptimeMs();

  Reading r;
  measure(r);
#if ENABLE_AERATOR_CONTROL
  // An uncalibrated DO probe must not switch the aerator off: treat it as unknown (fail-safe on).
  const float controlDo = calDoDone(cal) ? r.doMgL : notANumber();
  setAeratorRelay(aerator.update(controlDo, uptimeMs() / 1000UL));
#endif

  const SensorPacket packet = buildPacket(r);
  const bool ok = transmit(packet);
  previousTxFailed = !ok;
  ++sequence;
  printCsv(packet, ok);

  const uint32_t intervalMs = SEND_INTERVAL_S * 1000UL;
  const uint32_t elapsedMs = uptimeMs() - cycleStartMs;
  if (elapsedMs < intervalMs) waitUntilNextCycle(intervalMs - elapsedMs);
}
