#include "Aerator.h"

#include "WaterQuality.h"

namespace pond {

AeratorController::AeratorController(const AeratorConfig& config)
    : config_(config), on_(true), lastChangeSeconds_(0) {}

void AeratorController::begin(uint32_t nowSeconds) {
  on_ = true;
  lastChangeSeconds_ = nowSeconds;
}

bool AeratorController::update(float doMgL, uint32_t nowSeconds) {
  if (isNan(doMgL)) {
    if (!on_) {
      on_ = true;
      lastChangeSeconds_ = nowSeconds;
    }
    return on_;
  }
  if (!on_ && doMgL < config_.doLowMgL) {
    on_ = true;  // low oxygen: start immediately
    lastChangeSeconds_ = nowSeconds;
  } else if (on_ && doMgL > config_.doHighMgL &&
             static_cast<uint32_t>(nowSeconds - lastChangeSeconds_) >= config_.minOnSeconds) {
    on_ = false;
    lastChangeSeconds_ = nowSeconds;
  }
  return on_;
}

}  // namespace pond
