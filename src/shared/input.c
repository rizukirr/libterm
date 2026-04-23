#include "internal.h"
#include "platform.h"

int lt_poll_event(struct lt_event *event) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  return lt__plat_read_event(event, -1);
}

int lt_peek_event(struct lt_event *event, int timeout_ms) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  return lt__plat_read_event(event, timeout_ms);
}

int lt_set_input_mode(int mode) {
  if (mode == LT_INPUT_CURRENT)
    return lt__g.input_mode;
  lt__g.input_mode = mode;
  return mode;
}
