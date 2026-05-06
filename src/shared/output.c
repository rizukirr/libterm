#include "internal.h"
#include "intrinsics/diff.h"
#include "platform.h"

int lt_clear(void) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;

  if (!lt__g.back || lt__g.width <= 0 || lt__g.height <= 0)
    return LT_ERR_NOT_INIT;

  const int count = lt__g.width * lt__g.height;
  lt__buffer_clear(lt__g.back, count, lt__g.clear_fg, lt__g.clear_bg);

  for (int y = 0; y < lt__g.height; y++)
    lt__g.dirty_rows[y] = true;

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

  lt__g.cur_x = -1;
  lt__g.cur_y = -1;

  size_t idx = 0;
  int mc = 0, rc = 0, rest = 0, run_len = 0, run_end = 0, skip = 0;

  for (int y = 0; y < lt__g.height; y++) {
    if (!lt__g.dirty_rows[y])
      continue;

    int x = 0;
    while (x < lt__g.width) {
      /* SIMD outer skip: advance past all equal cells */
      skip = lt__simd_diff_first_differ_cell(&lt__g.back[y * lt__g.width + x],
                                             &lt__g.front[y * lt__g.width + x],
                                             lt__g.width - x);
      x += skip;
      if (x >= lt__g.width)
        break;

      idx = (size_t)(y * lt__g.width + x);

      /* SIMD inner walk: find end of changed run by locating first equal */
      rest = lt__simd_diff_first_equal_cell(
          &lt__g.back[idx + 1], &lt__g.front[idx + 1], lt__g.width - x - 1);
      run_len = 1 + rest;
      run_end = x + run_len;

      /* emit cursor jump if discontinuous from cache */
      if (x != lt__g.cur_x || y != lt__g.cur_y) {
        mc = lt__plat_move_cursor(x, y);
        if (mc != LT_OK) {
          (void)lt__plat_flush();
          return mc;
        }
      }

      /* emit the entire run in one block */
      rc = lt__plat_render_run(&lt__g.back[idx], run_len);
      if (rc != LT_OK) {
        (void)lt__plat_flush();
        return rc;
      }

      /* update cache for end-of-run position */
      lt__g.cur_x = x + run_len;
      lt__g.cur_y = y;
      if (lt__g.cur_x >= lt__g.width) {
        lt__g.cur_x = -1;
        lt__g.cur_y = -1;
      }

      /* sync front for all cells in run */
      for (int i = 0; i < run_len; i++)
        lt__g.front[idx + i] = lt__g.back[idx + i];

      x = run_end;
    }

    lt__g.dirty_rows[y] = false;
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

int lt_show_cursor(void) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  return lt__plat_show_cursor();
}

int lt_set_output_mode(int mode) {
  if (mode == LT_OUTPUT_CURRENT)
    return lt__g.output_mode;
  lt__g.output_mode = mode;
  return mode;
}
