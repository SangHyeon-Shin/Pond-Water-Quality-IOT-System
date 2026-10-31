// Aerator (air diffuser) on/off decision from dissolved oxygen, with hysteresis.
// Fail-safe: whenever DO is unknown the aerator runs.
#pragma once

#include <stdint.h>

namespace pond {

struct AeratorConfig {
  float doLowMgL;          // switch ON below this
  float doHighMgL;         // switch OFF above this (must be > doLowMgL)
  uint32_t minOnSeconds;   // minimum run time before switching OFF again
};

class AeratorController {
 public:
  explicit AeratorController(const AeratorConfig& config);
  void begin(uint32_t nowSeconds);                    // starts ON
  bool update(float doMgL, uint32_t nowSeconds);      // returns true while the aerator should run
  bool isOn() const { return on_; }

 private:
  AeratorConfig config_;
  bool on_;
  uint32_t lastChangeSeconds_;
};

}  // namespace pond
