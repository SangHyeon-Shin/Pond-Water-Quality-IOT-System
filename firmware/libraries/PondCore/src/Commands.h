// Serial maintenance commands of the pond node (see docs/calibration.md).
//
//   help | ?            list commands
//   status | s          print raw voltages, values and calibration
//   exit | q            leave maintenance mode
//   cal ph1 <pH>        store current pH voltage as point 1 (e.g. "cal ph1 7.00")
//   cal ph2 <pH>        store current pH voltage as point 2 (e.g. "cal ph2 4.00")
//   cal tds <ppm>       compute k from a standard (e.g. "cal tds 707")
//   cal ntu0            current turbidity voltage = 0 NTU (clear water)
//   cal ntu <NTU>       current turbidity voltage = standard (e.g. "cal ntu 100")
//   cal temp <C>        reference thermometer reading -> temperature offset
//   cal vcc <V>         multimeter reading of the 5V pin -> internal reference
//   cal do1 | cal do2   current DO voltage = air saturation at the current temperature
//   cal reset           restore defaults
#pragma once

#include <stdint.h>

namespace pond {

enum CommandType : uint8_t {
  CMD_NONE,
  CMD_INVALID,
  CMD_HELP,
  CMD_STATUS,
  CMD_EXIT,
  CMD_CAL_PH,
  CMD_CAL_TDS,
  CMD_CAL_NTU_ZERO,
  CMD_CAL_NTU,
  CMD_CAL_TEMP,
  CMD_CAL_VCC,
  CMD_CAL_DO,
  CMD_CAL_RESET
};

struct Command {
  CommandType type;
  uint8_t point;  // 1 or 2 for "cal ph" / "cal do"
  float value;
};

// Case-insensitive; extra spaces are ignored. Lines longer than 47 characters are invalid.
Command parseCommand(const char* line);

}  // namespace pond
