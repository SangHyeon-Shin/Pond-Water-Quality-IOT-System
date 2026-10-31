// Gateway for the 2024 hardware: Arduino Uno + ESP-01 (AT firmware) + nRF24L01.
// Receives packets from the pond node and forwards them to Blynk.
// Recommended replacement with fewer parts: ../gateway_nodemcu
//
// Setup: docs/blynk-setup.md. Copy secrets.example.h to secrets.h first.
// Libraries: RF24 1.6.2, Blynk 1.4.0, BlynkESP8266 (ESP8266_Lib), PondCore (this repository)
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy secrets.example.h to secrets.h and fill in your Blynk and Wi-Fi credentials"
#endif

#include "gateway_config.h"

#if GATEWAY_BLYNK_DEBUG
#define BLYNK_PRINT Serial  // Blynk connection log on the serial monitor
#endif

#include <SoftwareSerial.h>
#include <ESP8266_Lib.h>
#include <BlynkSimpleShieldEsp8266.h>
#include <PondCore.h>
#include <PondGateway.h>
#include <RF24.h>
#include <SPI.h>

SoftwareSerial espSerial(PIN_ESP_RX, PIN_ESP_TX);
ESP8266 wifi(&espSerial);
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
bool wifiJoined = false;
uint32_t lastWifiAttemptMs = 0;

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

void maintainNetwork() {
  if (Blynk.connected()) return;
  // Blynk.run() reconnects to the cloud on its own; re-join Wi-Fi when that keeps failing.
  if (!wifiJoined || millis() - lastWifiAttemptMs >= WIFI_RETRY_INTERVAL_S * 1000UL) {
    lastWifiAttemptMs = millis();
    wifiJoined = Blynk.connectWiFi(WIFI_SSID, WIFI_PASS);
  }
}

void checkNode() { app.checkNode(millis()); }

void setup() {
  Serial.begin(SERIAL_BAUD);
  espSerial.begin(ESP8266_BAUD);
  Serial.println(F("Pond gateway (Uno + ESP-01) 2.0.0"));

  checkRadio();
  Blynk.config(wifi, BLYNK_AUTH_TOKEN);  // non-blocking: no endless wait if Wi-Fi is down
  maintainNetwork();

  timer.setInterval(5000L, checkNode);
  timer.setInterval(10000L, maintainNetwork);
  timer.setInterval(RADIO_CHECK_INTERVAL_S * 1000L, checkRadio);
  app.printCsvHeader();
}

void loop() {
  if (radioReady) app.pollRadio(millis());
  Blynk.run();
  timer.run();
}
