// Gateway (Arduino Uno + ESP-01) configuration. Radio settings are shared with the
// node in firmware/libraries/PondCore/src/PondProtocol.h.
#pragma once

// ---- Pins ------------------------------------------------------------------
#define PIN_ESP_RX 2  // Uno RX (SoftwareSerial) <- ESP-01 TX
#define PIN_ESP_TX 3  // Uno TX (SoftwareSerial) -> ESP-01 RX (through the adapter board)
#define PIN_RF_CE 7
#define PIN_RF_CSN 8
// D11/D12/D13: SPI bus of the nRF24L01.

// ESP-01 AT firmware baud rate. SoftwareSerial is only reliable at 9600; set the
// module once with AT+UART_DEF=9600,8,1,0,0 (docs/blynk-setup.md).
#define ESP8266_BAUD 9600
#define SERIAL_BAUD 9600

// Blynk connection log on the serial monitor. 0 saves about 1.2 KB of flash
// (RAM use is the same: 1265 bytes of 2048 with either setting).
#define GATEWAY_BLYNK_DEBUG 1

// ---- Behaviour -----------------------------------------------------------------
#define NODE_OFFLINE_TIMEOUT_S 300  // > 3 node intervals (node default: 60 s)
#define WIFI_RETRY_INTERVAL_S 60
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
