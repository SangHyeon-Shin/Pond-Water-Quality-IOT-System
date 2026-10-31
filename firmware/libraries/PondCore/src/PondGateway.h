// Gateway application shared by both gateway sketches. Header-only template so that
// it can use the sketch's RF24, Blynk and Serial objects without this library
// depending on them. Include it after the Blynk header in a gateway sketch.
#pragma once

#include "GatewayLogic.h"
#include "PondProtocol.h"
#include "WaterQuality.h"

#ifndef F
#define F(text) (text)
#endif

namespace pond {

// Blynk datastreams (virtual pins). V0-V3 are the same as on the 2024 dashboard.
// Keep in sync with docs/blynk-setup.md.
enum VirtualPin : uint8_t {
  VPIN_TEMPERATURE = 0,
  VPIN_PH = 1,
  VPIN_TURBIDITY = 2,
  VPIN_TDS = 3,
  VPIN_DO = 4,
  VPIN_AERATOR = 5,
  VPIN_NODE_VCC = 6,
  VPIN_STATUS = 7,
  VPIN_NODE_ONLINE = 8,
  VPIN_PACKET_LOSS = 9
};

// Event codes; create events with exactly these codes in the Blynk template.
#define POND_EVENT_ALERT "water_quality_alert"
#define POND_EVENT_OFFLINE "node_offline"
#define POND_EVENT_ONLINE "node_online"

template <typename RadioT, typename BlynkT, typename LogT>
class GatewayApp {
 public:
  GatewayApp(RadioT& radio, BlynkT& blynk, LogT& log, uint32_t offlineTimeoutMs, const AlertThresholds& thresholds)
      : radio_(radio), blynk_(blynk), log_(log), logic_(offlineTimeoutMs, thresholds) {}

  void printCsvHeader() {
    log_.println(F("# CSV,ms,seq,temp_c,ph,turbidity_ntu,tds_ppm,do_mgl,aerator,vcc_v,status_hex,loss_pct"));
  }

  // Drains the radio FIFO and forwards every valid packet. Call from every loop().
  void pollRadio(uint32_t nowMs) {
    uint8_t buffer[PACKET_SIZE];
    while (radio_.available()) {
      radio_.read(buffer, PACKET_SIZE);
      SensorPacket packet;
      const PacketResult result = logic_.accept(buffer, PACKET_SIZE, nowMs, packet);
      if (result == PACKET_OK) {
        forward(packet, nowMs);
      } else if (result == PACKET_BAD_VERSION) {
        log_.println(F("# ignored a packet with another protocol version (update the node)"));
      }
    }
  }

  // Reports node online/offline changes. Call every few seconds.
  void checkNode(uint32_t nowMs) {
    const int8_t change = logic_.updateOnline(nowMs);
    if (change == 0) return;
    blynk_.virtualWrite(VPIN_NODE_ONLINE, change > 0 ? 1 : 0);
    if (change > 0) {
      blynk_.logEvent(POND_EVENT_ONLINE);
      log_.println(F("# pond node online"));
    } else {
      blynk_.logEvent(POND_EVENT_OFFLINE);
      log_.println(F("# pond node OFFLINE: no packets received"));
    }
  }

  const GatewayLogic& logic() const { return logic_; }

 private:
  void writeIfValid(uint8_t pin, float value) {
    if (!isNan(value)) blynk_.virtualWrite(pin, value);  // never send NaN to the dashboard
  }

  void forward(const SensorPacket& p, uint32_t nowMs) {
    writeIfValid(VPIN_TEMPERATURE, p.temperatureC);
    writeIfValid(VPIN_PH, p.pH);
    writeIfValid(VPIN_TURBIDITY, p.turbidityNtu);
    writeIfValid(VPIN_TDS, p.tdsPpm);
    writeIfValid(VPIN_DO, p.dissolvedOxygenMgL);
    const int aerator = p.aeratorOn;
    if (aerator != AERATOR_UNCONTROLLED) blynk_.virtualWrite(VPIN_AERATOR, aerator);
    const float vcc = p.vccMillivolts / 1000.0f;
    blynk_.virtualWrite(VPIN_NODE_VCC, vcc);
    const unsigned int status = p.status;
    blynk_.virtualWrite(VPIN_STATUS, status);
    blynk_.virtualWrite(VPIN_PACKET_LOSS, logic_.lossPercent());

    char message[64];
    if (logic_.checkAlerts(p, nowMs, message, sizeof(message)) != 0) {
      blynk_.logEvent(POND_EVENT_ALERT, message);
      log_.print(F("# alert: "));
      log_.println(message);
    }
    printCsv(p, nowMs);
  }

  void printCsv(const SensorPacket& p, uint32_t nowMs) {
    const unsigned long ms = nowMs;
    const unsigned int seq = p.seq;
    const unsigned int aerator = p.aeratorOn;
    const unsigned int status = p.status;
    log_.print(F("CSV,"));
    log_.print(ms);
    log_.print(',');
    log_.print(seq);
    log_.print(',');
    log_.print(p.temperatureC, 2);
    log_.print(',');
    log_.print(p.pH, 2);
    log_.print(',');
    log_.print(p.turbidityNtu, 1);
    log_.print(',');
    log_.print(p.tdsPpm, 0);
    log_.print(',');
    log_.print(p.dissolvedOxygenMgL, 2);
    log_.print(',');
    log_.print(aerator);
    log_.print(',');
    log_.print(p.vccMillivolts / 1000.0f, 3);
    log_.print(F(",0x"));
    log_.print(status, 16);
    log_.print(',');
    log_.println(logic_.lossPercent(), 1);
  }

  RadioT& radio_;
  BlynkT& blynk_;
  LogT& log_;
  GatewayLogic logic_;
};

}  // namespace pond
