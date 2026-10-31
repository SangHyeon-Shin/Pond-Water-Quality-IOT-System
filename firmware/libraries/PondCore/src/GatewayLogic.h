// Gateway-side packet handling: validation, loss statistics, node online/offline
// detection and threshold alerts. Independent of Blynk so it can be unit-tested.
#pragma once

#include <stdint.h>

#include "PondProtocol.h"

namespace pond {

// Example thresholds for a fish pond; tune them for your site.
struct AlertThresholds {
  float phLow, phHigh;
  float temperatureHighC;
  float doLowMgL;
  float turbidityHighNtu;
  float tdsHighPpm;
  float vccLowV;
  uint32_t cooldownMs;  // minimum time between two alerts of the same kind
};
AlertThresholds defaultAlertThresholds();

enum AlertBit : uint8_t {
  ALERT_PH = 1u << 0,
  ALERT_TEMP = 1u << 1,
  ALERT_DO = 1u << 2,
  ALERT_TURBIDITY = 1u << 3,
  ALERT_TDS = 1u << 4,
  ALERT_VCC = 1u << 5
};
const uint8_t ALERT_KINDS = 6;

enum PacketResult : uint8_t { PACKET_OK, PACKET_BAD_SIZE, PACKET_BAD_VERSION, PACKET_DUPLICATE };

class GatewayLogic {
 public:
  GatewayLogic(uint32_t offlineTimeoutMs, const AlertThresholds& thresholds);

  // Validates a received payload and updates the statistics.
  PacketResult accept(const void* data, uint8_t length, uint32_t nowMs, SensorPacket& out);

  // +1 when the node has just come online, -1 when it has just gone offline, else 0.
  int8_t updateOnline(uint32_t nowMs);
  bool online() const { return online_; }

  uint32_t received() const { return received_; }
  uint32_t lost() const { return lost_; }
  float lossPercent() const;

  // Alerts that are due now (cooldown respected), as AlertBit mask. Values from
  // invalid or uncalibrated sensors never raise alerts. Writes a short summary to msg.
  uint8_t checkAlerts(const SensorPacket& p, uint32_t nowMs, char* msg, uint8_t msgSize);

 private:
  bool due(uint8_t index, uint32_t nowMs) const;

  uint32_t offlineTimeoutMs_;
  AlertThresholds thresholds_;
  bool online_;
  bool haveRx_;
  bool haveSeq_;
  uint16_t lastSeq_;
  uint32_t lastRxMs_;
  uint32_t received_;
  uint32_t lost_;
  uint8_t alertedMask_;
  uint32_t lastAlertMs_[ALERT_KINDS];
};

}  // namespace pond
