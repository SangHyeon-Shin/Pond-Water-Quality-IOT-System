// Small robust-averaging helpers for ADC samples (no dynamic memory).
#pragma once

#include <stdint.h>

namespace pond {

// In-place insertion sort.
void sortU16(uint16_t* values, uint8_t count);

// Median of values[0..count-1]. Sorts the array. NaN when count == 0.
float medianU16(uint16_t* values, uint8_t count);

// Mean after dropping `trim` lowest and `trim` highest samples. Sorts the array.
// Uses the plain mean when count <= 2 * trim. NaN when count == 0.
float trimmedMeanU16(uint16_t* values, uint8_t count, uint8_t trim);

}  // namespace pond
