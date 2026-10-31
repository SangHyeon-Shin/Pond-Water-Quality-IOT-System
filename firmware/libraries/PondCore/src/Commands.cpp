#include "Commands.h"

#include <stdlib.h>
#include <string.h>

namespace pond {

static const uint8_t MAX_LINE = 48;
static const uint8_t MAX_TOKENS = 4;

static bool parseNumber(const char* text, float& out) {
  if (text == 0 || *text == '\0') return false;
  char* end = 0;
  const double value = strtod(text, &end);
  if (end == text || *end != '\0') return false;
  out = static_cast<float>(value);
  return true;
}

static Command make(CommandType type, uint8_t point = 0, float value = 0.0f) {
  Command c;
  c.type = type;
  c.point = point;
  c.value = value;
  return c;
}

Command parseCommand(const char* line) {
  if (line == 0) return make(CMD_NONE);
  if (strlen(line) >= MAX_LINE) return make(CMD_INVALID);

  char buf[MAX_LINE];
  uint8_t len = 0;
  for (const char* p = line; *p != '\0'; ++p) {
    char ch = *p;
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    if (ch == '\t' || ch == '\r' || ch == '\n') ch = ' ';
    buf[len++] = ch;
  }
  buf[len] = '\0';

  char* tokens[MAX_TOKENS];
  uint8_t count = 0;
  char* p = buf;
  while (*p != '\0') {
    while (*p == ' ') *p++ = '\0';
    if (*p == '\0') break;
    if (count == MAX_TOKENS) return make(CMD_INVALID);
    tokens[count++] = p;
    while (*p != '\0' && *p != ' ') ++p;
  }
  if (count == 0) return make(CMD_NONE);

  const char* t0 = tokens[0];
  if (count == 1) {
    if (!strcmp(t0, "help") || !strcmp(t0, "?")) return make(CMD_HELP);
    if (!strcmp(t0, "status") || !strcmp(t0, "s")) return make(CMD_STATUS);
    if (!strcmp(t0, "exit") || !strcmp(t0, "quit") || !strcmp(t0, "q")) return make(CMD_EXIT);
    return make(CMD_INVALID);
  }
  if (strcmp(t0, "cal") != 0 || count > 3) return make(CMD_INVALID);

  const char* what = tokens[1];
  float value = 0.0f;
  const bool hasValue = (count == 3) && parseNumber(tokens[2], value);
  if (count == 3 && !hasValue) return make(CMD_INVALID);

  if (count == 2) {
    if (!strcmp(what, "ntu0")) return make(CMD_CAL_NTU_ZERO);
    if (!strcmp(what, "do1")) return make(CMD_CAL_DO, 1);
    if (!strcmp(what, "do2")) return make(CMD_CAL_DO, 2);
    if (!strcmp(what, "reset")) return make(CMD_CAL_RESET);
    return make(CMD_INVALID);
  }
  if (!strcmp(what, "ph1")) return make(CMD_CAL_PH, 1, value);
  if (!strcmp(what, "ph2")) return make(CMD_CAL_PH, 2, value);
  if (!strcmp(what, "tds")) return make(CMD_CAL_TDS, 0, value);
  if (!strcmp(what, "ntu")) return value == 0.0f ? make(CMD_CAL_NTU_ZERO) : make(CMD_CAL_NTU, 0, value);
  if (!strcmp(what, "temp")) return make(CMD_CAL_TEMP, 0, value);
  if (!strcmp(what, "vcc")) return make(CMD_CAL_VCC, 0, value);
  return make(CMD_INVALID);
}

}  // namespace pond
