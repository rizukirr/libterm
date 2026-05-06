#include "common.h"
#include "diff.h"

int lt__simd_diff_first_differ_cell(const struct lt_cell *a,
                                    const struct lt_cell *b, int count) {
  for (int i = 0; i < count; i++)
    if (a[i].ch != b[i].ch || a[i].fg != b[i].fg || a[i].bg != b[i].bg)
      return i;
  return count;
}

int lt__simd_diff_first_equal_cell(const struct lt_cell *a,
                                   const struct lt_cell *b, int count) {
  for (int i = 0; i < count; i++)
    if (a[i].ch == b[i].ch && a[i].fg == b[i].fg && a[i].bg == b[i].bg)
      return i;

  return count;
}
