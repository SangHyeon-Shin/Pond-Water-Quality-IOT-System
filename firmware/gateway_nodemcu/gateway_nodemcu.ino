// Recommended gateway: one NodeMCU V2 (ESP-12, already in the BOM) + nRF24L01.
// Replaces the Arduino Uno + ESP-01 pair: Wi-Fi is built in, no AT firmware,
// no SoftwareSerial, and 80 KB of RAM instead of 2 KB.
//
// Setup: docs/blynk-setup.md, wiring: docs/hardware.md. Copy secrets.example.h to secrets.h first.
// Board: "NodeMCU 1.0 (ESP-12E Module)" from the ESP8266 core 3.1.2.
// Libraries: RF24 1.6.2, Blynk 1.4.0, PondCore (this repository)
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy secrets.example.h to secrets.h and fill in your Blynk and Wi-Fi credentials"
#endif

#include "gateway_config.h"

#if GATEWAY_BLYNK_DEBUG
#define BLYNK_PRINT Serial
#endif

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <PondCore.h>
#include <PondGateway.h>
#include <RF24.h>
#include <SPI.h>

RF24 radio(PIN_RF_CE, PIN_RF_CSN);
BlynkTimer timer;

pond::AlertThresholds alertThresholds() {
  pond::AlertThresholds t = pond::defaultAlertThresholds();
  t.phLow = ALERT_PH_LOW;
  t.phHigh = ALERT_PH_HIGH;
  t.temperatureHighC = ALERT_TEMP_HIGH_C;
  t.doLowMgL = ALERT_DO_LOW_MGL;
  t.turbidityHighNtu = ALERT_TURBIDITY_HIGH_NTU;
  t.tdsHighPpm = ALERT_TDS_HIGH_PPM;
  t.vccLowV = ALERT_NODE_VCC_LOW_V;
  t.cooldownMs = ALERT_COOLDOWN_MIN * 60UL * 1000UL;
  return t;
}

pond::GatewayApp<RF24, BlynkWifi, HardwareSerial> app(radio, Blynk, Serial, NODE_OFFLINE_TIMEOUT_S * 1000UL,
                                                     alertThresholds());

bool radioReady = false;

bool setupRadio() {
  if (!radio.begin() || !radio.isChipConnected()) return false;
  radio.setChannel(POND_RF_CHANNEL);
  if (!radio.setDataRate(POND_RF_DATA_RATE)) Serial.println(F("# radio: data rate not supported by this chip"));
  radio.setPALevel(POND_RF_PA_LEVEL);
  radio.setPayloadSize(pond::PACKET_SIZE);
  radio.setAutoAck(true);
  radio.openReadingPipe(1, reinterpret_cast<const uint8_t*>(POND_RF_ADDRESS));
  radio.startListening();
  return true;
}

void checkRadio() {
  if (radioReady && radio.isChipConnected()) return;
  radioReady = setupRadio();
  Serial.println(radioReady ? F("# radio ready") : F("# nRF24L01 not responding: check wiring and the 3.3 V adapter"));
}

void checkNode() { app.checkNode(millis()); }

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println();
  Serial.println(F("Pond gateway (NodeMCU) 2.0.0"));

  checkRadio();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);      // non-blocking; Blynk.run() connects once Wi-Fi is up
  Blynk.config(BLYNK_AUTH_TOKEN);

  timer.setInterval(5000L, checkNode);
  timer.setInterval(RADIO_CHECK_INTERVAL_S * 1000L, checkRadio);
  app.printCsvHeader();
}

void loop() {
  if (radioReady) app.pollRadio(millis());
  if (WiFi.status() == WL_CONNECTED) Blynk.run();
  timer.run();
}
