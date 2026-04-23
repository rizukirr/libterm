#define ARENA_IMPLEMENTATION
#include "internal.h"
#include "lib/arena.h"
#include <stdlib.h>

#define ARENA_BYTES_INIT 4096

int lt__buffer_resize(int w, int h) {
  if (w <= 0 || h <= 0)
    return LT_ERR;

  const size_t n = (size_t)(w * h);
  if (n == 0 || n > (SIZE_MAX / sizeof(struct lt_cell)))
    return LT_ERR_MEM;

  const size_t cells_bytes = n * sizeof(struct lt_cell);
  if (!lt__g.arena) {
    size_t arena_bytes = cells_bytes * 2;
    if (arena_bytes < ARENA_BYTES_INIT)
      arena_bytes = ARENA_BYTES_INIT;

    lt__g.arena = arena_create(arena_bytes);
    if (!lt__g.arena)
      return LT_ERR_MEM;
  }

  arena_reset(lt__g.arena);

  struct lt_cell *base =
      arena_alloc(lt__g.arena, cells_bytes, ARENA_ALIGNOF(struct lt_cell));
  if (!base)
    return LT_ERR_MEM;

  lt__g.back = base;
  lt__g.front = base + n;

  lt__g.width = w;
  lt__g.height = h;

  lt__buffer_clear(lt__g.back, (int)n, lt__g.clear_fg, lt__g.clear_bg);
  lt__buffer_clear(lt__g.front, (int)n, lt__g.clear_fg, lt__g.clear_bg);

  return LT_OK;
}

void lt__buffer_free(void) {
  if (lt__g.arena)
    arena_free(lt__g.arena);

  lt__g.arena = NULL;
  lt__g.back = NULL;
  lt__g.front = NULL;
  lt__g.width = 0;
  lt__g.height = 0;
}

void lt__buffer_clear(struct lt_cell *buf, int count, lt_attr fg, lt_attr bg) {
  for (int i = 0; i < count; i++) {
    buf[i].ch = ' ';
    buf[i].fg = fg;
    buf[i].bg = bg;
  }
}
