#include "TextFormat.h"

#include <string.h>

#include "WaterQuality.h"

namespace pond {

static char* copyText(char* buf, uint8_t size, const char* text) {
  if (size == 0) return buf;
  strncpy(buf, text, size - 1);
  buf[size - 1] = '\0';
  return buf;
}

char* formatFloat(char* buf, uint8_t size, float value, uint8_t decimals) {
  if (size == 0) return buf;
  if (isNan(value)) return copyText(buf, size, "nan");
  if (decimals > 4) decimals = 4;
  uint32_t scale = 1;
  for (uint8_t i = 0; i < decimals; ++i) scale *= 10;
  const bool negative = value < 0.0f;
  const float magnitude = negative ? -value : value;
  if (!(magnitude * static_cast<float>(scale) < 4.0e9f)) return copyText(buf, size, "ovf");

  const uint32_t scaled = static_cast<uint32_t>(magnitude * static_cast<float>(scale) + 0.5f);
  uint32_t whole = scaled / scale;
  uint32_t fraction = scaled % scale;

  char reversed[24];  // up to 4 fraction digits + '.' + 10 integer digits + '-'
  uint8_t n = 0;
  for (uint8_t i = 0; i < decimals; ++i) {
    reversed[n++] = static_cast<char>('0' + fraction % 10);
    fraction /= 10;
  }
  if (decimals > 0) reversed[n++] = '.';
  do {
    reversed[n++] = static_cast<char>('0' + whole % 10);
    whole /= 10;
  } while (whole > 0 && n < sizeof(reversed) - 1);
  if (negative && scaled != 0) reversed[n++] = '-';

  uint8_t length = 0;
  while (n > 0 && length + 1 < size) buf[length++] = reversed[--n];
  buf[length] = '\0';
  return buf;
}

void appendText(char* buf, uint8_t size, const char* text) {
  if (size == 0) return;
  const size_t used = strlen(buf);
  if (used + 1 >= size) return;
  strncat(buf, text, size - 1 - used);
}

}  // namespace pond
