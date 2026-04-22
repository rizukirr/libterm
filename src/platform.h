/*
 * libterm - platform abstraction interface.
 *
 * Shared code in src/shared/ calls these functions. Each platform
 * directory (src/platform/posix, src/platform/windows) provides its
 * own implementation. Shared code MUST NOT use #ifdef _WIN32 — add
 * a new lt__plat_ function here instead.
 */
#ifndef LIBTERM_PLATFORM_H
#define LIBTERM_PLATFORM_H

#include "libterm/libterm.h"
#include <stddef.h>

/* ---- lifecycle ---- */
int  lt__plat_init(void);
int  lt__plat_shutdown(void);

/* ---- terminal size ---- */
int  lt__plat_get_size(int *w, int *h);

/* ---- output ---- */
int  lt__plat_write(const char *buf, size_t len);
int  lt__plat_flush(void);
int  lt__plat_clear_screen(void);
int  lt__plat_move_cursor(int x, int y);
int  lt__plat_hide_cursor(void);
int  lt__plat_show_cursor(void);
int  lt__plat_render_cell(int x, int y, const struct lt_cell *cell);

/* ---- input ----
 * lt__plat_read_event: blocks up to timeout_ms (-1 = wait forever, 0 = nonblock)
 * returns LT_OK on event filled, LT_ERR_NO_EVENT on timeout, negative on error
 */
int  lt__plat_read_event(struct lt_event *ev, int timeout_ms);

#endif /* LIBTERM_PLATFORM_H */
