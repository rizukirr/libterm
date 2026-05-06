#include "internal.h"

int lt_set_cell(int x, int y, lt_uchar ch, lt_attr fg, lt_attr bg) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;

  if (!lt__g.back || x < 0 || y < 0 || x >= lt__g.width || y >= lt__g.height)
    return LT_ERR_OUT_OF_BOUNDS;

  lt__g.dirty_rows[y] = true;

  const size_t idx = (size_t)(y * lt__g.width + x);
  lt__g.back[idx].ch = ch;
  lt__g.back[idx].fg = fg;
  lt__g.back[idx].bg = bg;

  return LT_OK;
}
