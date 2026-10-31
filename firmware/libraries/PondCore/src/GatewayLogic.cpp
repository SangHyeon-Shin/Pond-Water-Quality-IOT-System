#include "GatewayLogic.h"

#include <string.h>

#include "TextFormat.h"
#include "WaterQuality.h"

namespace pond {

AlertThresholds defaultAlertThresholds() {
  AlertThresholds t;
  t.phLow = 6.0f;
  t.phHigh = 9.0f;
  t.temperatureHighC = 30.0f;
  t.doLowMgL = 4.0f;
  t.turbidityHighNtu = 100.0f;
  t.tdsHighPpm = 1000.0f;
  t.vccLowV = 4.5f;
  t.cooldownMs = 6UL * 60UL * 60UL * 1000UL;  // 6 h
  return t;
}

GatewayLogic::GatewayLogic(uint32_t offlineTimeoutMs, const AlertThresholds& thresholds)
    : offlineTimeoutMs_(offlineTimeoutMs),
      thresholds_(thresholds),
      online_(false),
      haveRx_(false),
      haveSeq_(false),
      lastSeq_(0),
      lastRxMs_(0),
      received_(0),
      lost_(0),
      alertedMask_(0) {
  for (uint8_t i = 0; i < ALERT_KINDS; ++i) lastAlertMs_[i] = 0;
}

PacketResult GatewayLogic::accept(const void* data, uint8_t length, uint32_t nowMs, SensorPacket& out) {
  if (length < PACKET_SIZE) return PACKET_BAD_SIZE;
  memcpy(&out, data, PACKET_SIZE);
  if (out.version != PROTOCOL_VERSION) return PACKET_BAD_VERSION;

  haveRx_ = true;
  lastRxMs_ = nowMs;
  if (haveSeq_ && out.seq == lastSeq_ && out.seq != 0) return PACKET_DUPLICATE;
  if (haveSeq_ && out.seq != 0) {  // seq == 0: node restarted, nothing was lost
    const uint16_t gap = static_cast<uint16_t>(out.seq - lastSeq_);
    if (gap > 1 && gap <= 1000) lost_ += gap - 1u;  // larger jumps: resynchronise silently
  }
  haveSeq_ = true;
  lastSeq_ = out.seq;
  ++received_;
  return PACKET_OK;
}

int8_t GatewayLogic::updateOnline(uint32_t nowMs) {
  const bool nowOnline = haveRx_ && static_cast<uint32_t>(nowMs - lastRxMs_) <= offlineTimeoutMs_;
  if (nowOnline == online_) return 0;
  online_ = nowOnline;
  return nowOnline ? 1 : -1;
}

float GatewayLogic::lossPercent() const {
  const uint32_t total = received_ + lost_;
  return total == 0 ? 0.0f : 100.0f * static_cast<float>(lost_) / static_cast<float>(total);
}

bool GatewayLogic::due(uint8_t index, uint32_t nowMs) const {
  if (!(alertedMask_ & (1u << index))) return true;
  return static_cast<uint32_t>(nowMs - lastAlertMs_[index]) >= thresholds_.cooldownMs;
}

static void appendValue(char* msg, uint8_t size, const char* label, float value, uint8_t decimals,
                        const char* unit) {
  char number[16];
  if (msg[0] != '\0') appendText(msg, size, "; ");
  appendText(msg, size, label);
  appendText(msg, size, formatFloat(number, sizeof(number), value, decimals));
  appendText(msg, size, unit);
}

uint8_t GatewayLogic::checkAlerts(const SensorPacket& p, uint32_t nowMs, char* msg, uint8_t msgSize) {
  if (msgSize > 0) msg[0] = '\0';
  const uint16_t st = p.status;
  const AlertThresholds& t = thresholds_;
  bool condition[ALERT_KINDS];
  condition[0] = !(st & (ST_PH_INVALID | ST_PH_UNCALIBRATED)) && !isNan(p.pH) && (p.pH < t.phLow || p.pH > t.phHigh);
  condition[1] = !(st & ST_TEMP_INVALID) && !isNan(p.temperatureC) && p.temperatureC > t.temperatureHighC;
  condition[2] = !(st & (ST_DO_INVALID | ST_DO_UNCALIBRATED)) && !isNan(p.dissolvedOxygenMgL) &&
                 p.dissolvedOxygenMgL < t.doLowMgL;
  condition[3] = !(st & (ST_TURB_INVALID | ST_TURB_UNCALIBRATED)) && !isNan(p.turbidityNtu) &&
                 p.turbidityNtu > t.turbidityHighNtu;
  condition[4] = !(st & (ST_TDS_INVALID | ST_TDS_UNCALIBRATED)) && !isNan(p.tdsPpm) && p.tdsPpm > t.tdsHighPpm;
  condition[5] = !(st & ST_VCC_UNCALIBRATED) && p.vccMillivolts > 0 &&
                 static_cast<float>(p.vccMillivolts) < t.vccLowV * 1000.0f;

  uint8_t raised = 0;
  for (uint8_t i = 0; i < ALERT_KINDS; ++i) {
    if (!condition[i] || !due(i, nowMs)) continue;
    raised |= static_cast<uint8_t>(1u << i);
    alertedMask_ |= static_cast<uint8_t>(1u << i);
    lastAlertMs_[i] = nowMs;
  }
  if (msgSize > 0) {
    if (raised & ALERT_PH) appendValue(msg, msgSize, "pH ", p.pH, 2, "");
    if (raised & ALERT_TEMP) appendValue(msg, msgSize, "Temp ", p.temperatureC, 1, " C");
    if (raised & ALERT_DO) appendValue(msg, msgSize, "DO ", p.dissolvedOxygenMgL, 1, " mg/L");
    if (raised & ALERT_TURBIDITY) appendValue(msg, msgSize, "Turbidity ", p.turbidityNtu, 0, " NTU");
    if (raised & ALERT_TDS) appendValue(msg, msgSize, "TDS ", p.tdsPpm, 0, " ppm");
    if (raised & ALERT_VCC) appendValue(msg, msgSize, "Node supply ", p.vccMillivolts / 1000.0f, 2, " V");
  }
  return raised;
}

}  // namespace pond
