#include "Filters.h"

#include "WaterQuality.h"

namespace pond {

void sortU16(uint16_t* values, uint8_t count) {
  for (uint8_t i = 1; i < count; ++i) {
    const uint16_t key = values[i];
    uint8_t j = i;
    while (j > 0 && values[j - 1] > key) {
      values[j] = values[j - 1];
      --j;
    }
    values[j] = key;
  }
}

float medianU16(uint16_t* values, uint8_t count) {
  if (count == 0) return notANumber();
  sortU16(values, count);
  if (count & 1) return values[count / 2];
  return (values[count / 2 - 1] + values[count / 2]) / 2.0f;
}

float trimmedMeanU16(uint16_t* values, uint8_t count, uint8_t trim) {
  if (count == 0) return notANumber();
  sortU16(values, count);
  uint8_t first = 0;
  uint8_t last = count;  // exclusive
  if (count > 2 * trim) {
    first = trim;
    last = count - trim;
  }
  uint32_t sum = 0;
  for (uint8_t i = first; i < last; ++i) sum += values[i];
  return static_cast<float>(sum) / static_cast<float>(last - first);
}

}  // namespace pond
