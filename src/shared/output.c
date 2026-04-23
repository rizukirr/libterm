#include "internal.h"
#include "platform.h"

int lt_clear(void) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;

  if (!lt__g.back || lt__g.width <= 0 || lt__g.height <= 0)
    return LT_ERR_NOT_INIT;

  const int count = lt__g.width * lt__g.height;
  lt__buffer_clear(lt__g.back, count, lt__g.clear_fg, lt__g.clear_bg);
  return LT_OK;
}

int lt_set_clear_attrs(lt_attr fg, lt_attr bg) {
  lt__g.clear_fg = fg;
  lt__g.clear_bg = bg;
  return LT_OK;
}

int lt_present(void) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;

  if (!lt__g.back || !lt__g.front || lt__g.width <= 0 || lt__g.height <= 0)
    return LT_ERR_NOT_INIT;

  for (int y = 0; y < lt__g.height; y++) {
    for (int x = 0; x < lt__g.width; x++) {
      const size_t idx = (size_t)(y * lt__g.width + x);

      int rc = lt__plat_render_cell(x, y, &lt__g.back[idx]);
      if (rc != LT_OK)
        return rc;

      lt__g.front[idx] = lt__g.back[idx];
    }
  }

  return lt__plat_flush();
}

int lt_set_cursor(int x, int y) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  return lt__plat_move_cursor(x, y);
}

int lt_hide_cursor(void) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  return lt__plat_hide_cursor();
}

int lt_set_output_mode(int mode) {
  if (mode == LT_OUTPUT_CURRENT)
    return lt__g.output_mode;
  lt__g.output_mode = mode;
  return mode;
}
