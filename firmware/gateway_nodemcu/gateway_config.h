// Gateway (NodeMCU V2 / ESP-12) configuration. Radio settings are shared with the
// node in firmware/libraries/PondCore/src/PondProtocol.h.
#pragma once

// ---- Pins ------------------------------------------------------------------
// nRF24L01 on the hardware SPI bus: SCK = D5, MISO = D6, MOSI = D7.
#define PIN_RF_CE D2   // GPIO4
#define PIN_RF_CSN D1  // GPIO5
#define SERIAL_BAUD 115200

// ---- Behaviour -----------------------------------------------------------------
#define GATEWAY_BLYNK_DEBUG 1       // Blynk connection log on the serial monitor
#define NODE_OFFLINE_TIMEOUT_S 300  // > 3 node intervals (node default: 60 s)
#define RADIO_CHECK_INTERVAL_S 60

// ---- Alerts (example values for a fish pond; tune for your site) ----------------
// Values from invalid or uncalibrated sensors never raise alerts.
#define ALERT_PH_LOW 6.0f
#define ALERT_PH_HIGH 9.0f
#define ALERT_TEMP_HIGH_C 30.0f
#define ALERT_DO_LOW_MGL 4.0f
#define ALERT_TURBIDITY_HIGH_NTU 100.0f
#define ALERT_TDS_HIGH_PPM 1000.0f
#define ALERT_NODE_VCC_LOW_V 4.5f
#define ALERT_COOLDOWN_MIN 360  // repeat a persisting alert at most every 6 h
