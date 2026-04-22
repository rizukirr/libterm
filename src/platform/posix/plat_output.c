#include "../../internal.h"
#include "../../platform.h"
#include <unistd.h>

/* TODO: output buffer, ANSI SGR encoding based on output_mode */
int lt__plat_write(const char *buf, size_t len) { (void)write(1, buf, len); return LT_OK; }
int lt__plat_flush(void)                         { return LT_OK; }
int lt__plat_clear_screen(void)                  { return LT_OK; }
int lt__plat_move_cursor(int x, int y)           { (void)x; (void)y; return LT_OK; }
int lt__plat_hide_cursor(void)                   { return LT_OK; }
int lt__plat_show_cursor(void)                   { return LT_OK; }
int lt__plat_render_cell(int x, int y, const struct lt_cell *c) { (void)x; (void)y; (void)c; return LT_OK; }
