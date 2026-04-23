#include "../../internal.h"
#include "../../platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* TODO: buffered writes via WriteFile to stdout handle, or WriteConsoleW
 *       + VT SGR sequences for color/attrs when VT processing is enabled.
 */
int lt__plat_write(const char *buf, size_t len) {
  DWORD written = 0;
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)len, &written, NULL);
  return LT_OK;
}
int lt__plat_flush(void) { return LT_OK; }
int lt__plat_clear_screen(void) { return LT_OK; }
int lt__plat_move_cursor(int x, int y) {
  (void)x;
  (void)y;
  return LT_OK;
}
int lt__plat_hide_cursor(void) { return LT_OK; }
int lt__plat_show_cursor(void) { return LT_OK; }
int lt__plat_render_cell(int x, int y, const struct lt_cell *c) {
  (void)x;
  (void)y;
  (void)c;
  return LT_OK;
}
