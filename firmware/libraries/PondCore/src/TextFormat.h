// Float-to-text without printf("%f"): AVR's printf has no float support.
#pragma once

#include <stdint.h>

namespace pond {

// Writes `value` with `decimals` (0..4) fraction digits into buf (always NUL-terminated).
// NaN -> "nan"; magnitudes that do not fit -> "ovf". Returns buf.
char* formatFloat(char* buf, uint8_t size, float value, uint8_t decimals);

// Appends `text` to the NUL-terminated string in buf without overflowing `size`.
void appendText(char* buf, uint8_t size, const char* text);

}  // namespace pond
