// Pond node configuration. Radio channel/address/data rate are shared with the
// gateways and live in firmware/libraries/PondCore/src/PondProtocol.h.
#pragma once

// ---- Identity and timing ---------------------------------------------------
#define NODE_ID 1
#define SEND_INTERVAL_S 60          // one measurement + packet per interval
#define ENABLE_SLEEP 1              // power-down between measurements (AVR watchdog, +/-10 % timing)
#define MAINTENANCE_WINDOW_MS 5000  // after reset: press Enter within this time for maintenance mode

// ---- Pins ------------------------------------------------------------------
#define PIN_TURBIDITY A0  // DFRobot SEN0189, switch on the board set to "A" (analog)
#define PIN_TDS A1        // DFRobot Gravity TDS
#define PIN_PH A2         // DFRobot pH board (V1 or V2, see PH_BOARD_VERSION)
#define PIN_DO A3         // optional DFRobot SEN0237 (ENABLE_DO_SENSOR)
#define PIN_DS18B20 2     // 4.7 kOhm pull-up to 5 V
#define PIN_RELAY 6       // optional aerator relay (ENABLE_AERATOR_CONTROL)
#define PIN_RF_CE 7
#define PIN_RF_CSN 8
// D11/D12/D13 are the SPI bus of the nRF24L01. Do not use D13 (on-board LED) for anything else.

// pH/TDS interference: the TDS probe drives an AC signal into the water that disturbs
// the pH electrode. Wire the TDS board VCC to a digital pin (it draws 3-6 mA) and set
// the pin here; the node then powers the TDS probe only while measuring TDS.
// -1 = TDS board permanently powered from 5 V (2024 wiring).
#define PIN_TDS_POWER -1
#define TDS_WARMUP_MS 2000

// ---- Sensors ---------------------------------------------------------------
// 1 = "pH meter V1.1" board (pH = 3.5 * V before calibration)
// 2 = Gravity pH V2 board (SEN0161-V2 / SEN0169-V2, pH 7 ~ 1.5 V before calibration)
// Unsure? Put the probe in pH 7 buffer: about 2.0 V -> 1, about 1.5 V -> 2.
#define PH_BOARD_VERSION 1
#define ENABLE_DO_SENSOR 0
#define TEMP_RESOLUTION_BITS 12  // DS18B20: 12 bit = 0.0625 C, 750 ms conversion

// Samples per measurement (robust mean drops the lowest and highest 10 %).
#define PH_SAMPLES 40
#define PH_SAMPLE_INTERVAL_MS 20
#define TDS_SAMPLES 30
#define TDS_SAMPLE_INTERVAL_MS 40
#define TURB_SAMPLES 20
#define TURB_SAMPLE_INTERVAL_MS 10
#define DO_SAMPLES 20
#define DO_SAMPLE_INTERVAL_MS 20

// Plausibility limits; readings outside are sent as NaN with an "invalid" status bit.
#define TEMP_MIN_VALID_C -5.0f
#define TEMP_MAX_VALID_C 45.0f
#define PH_VOLTS_MIN 0.05f
#define PH_VOLTS_MAX 4.95f
#define TDS_VOLTS_MAX 2.40f  // board output range 0-2.3 V
#define TURB_VOLTS_MIN 0.05f
#define TURB_VOLTS_MAX 4.95f
#define DO_VOLTS_MAX 3.30f
#define DO_MAX_VALID_MGL 25.0f

// ---- Aerator control (optional) -----------------------------------------------
// Needs ENABLE_DO_SENSOR. Without a valid DO reading the aerator always runs.
#define ENABLE_AERATOR_CONTROL 0
#define RELAY_ACTIVE_LOW 1                  // most 5 V relay modules switch on with IN = LOW
#define AERATOR_ON_WHEN_RELAY_ENERGIZED 0   // 0: aerator wired through the NC contact (keeps running if the node fails)
#define DO_LOW_MGL 5.0f                     // switch the aerator ON below this
#define DO_HIGH_MGL 7.0f                    // switch it OFF above this
#define AERATOR_MIN_ON_S 900UL              // minimum run time before switching OFF

// ---- Radio -------------------------------------------------------------------
#define RF_RETRY_DELAY 5   // (5 + 1) * 250 us = 1.5 ms between retries (needed at 250 kbps)
#define RF_RETRY_COUNT 15

// ---- Misc --------------------------------------------------------------------
#define NOMINAL_VCC 5.0f   // used until "cal vcc" has been run
#define CAL_EEPROM_ADDRESS 0
#define SERIAL_BAUD 9600
