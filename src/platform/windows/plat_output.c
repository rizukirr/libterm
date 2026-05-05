#include "internal.h"
#include "libterm/libterm.h"
#include "platform.h"
#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define LT__OUTBUF_CAP 8192

static char lt__outbuf[LT__OUTBUF_CAP];
static size_t lt__outbuf_len = 0;

static int lt__win_raw_write(const char *buf, size_t len) {
  if (len == 0)
    return LT_OK;

  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (out == NULL || out == INVALID_HANDLE_VALUE)
    return LT_ERR;

  DWORD written = 0;
  if (!WriteFile(out, buf, (DWORD)len, &written, NULL))
    return LT_ERR;

  if (written != (DWORD)len)
    return LT_ERR;

  return LT_OK;
}

static int lt__win_write_uint(char *buf, int v) {
  if (v < 0)
    v = 0;

  if (v == 0) {
    buf[0] = '0';
    return 1;
  }

  char tmp[10];
  int n = 0;
  while (v > 0) {
    tmp[n++] = '0' + (v % 10);
    v /= 10;
  }

  for (int i = 0; i < n; i++)
    buf[i] = tmp[n - i - 1];

  return n;
}

int lt__plat_write(const char *buf, size_t len) {
  if (len == 0)
    return LT_OK;

  if (len > LT__OUTBUF_CAP) {
    int rc = lt__plat_flush();
    if (rc != LT_OK)
      return rc;

    return lt__win_raw_write(buf, len);
  }

  if (lt__outbuf_len + len > LT__OUTBUF_CAP) {
    int rc = lt__plat_flush();
    if (rc != LT_OK)
      return rc;
  }

  memcpy(lt__outbuf + lt__outbuf_len, buf, len);
  lt__outbuf_len += len;

  return LT_OK;
}

int lt__plat_flush(void) {
  if (lt__outbuf_len == 0)
    return LT_OK;

  int rc = lt__win_raw_write(lt__outbuf, lt__outbuf_len);
  lt__outbuf_len = 0;
  return rc;
}

int lt__plat_clear_screen(void) {
  static const char seq[] = "\x1b[2J\x1b[H";
  return lt__plat_write(seq, sizeof(seq) - 1);
}

int lt__plat_move_cursor(int x, int y) {
  if (x < 0 || y < 0)
    return LT_ERR_OUT_OF_BOUNDS;

  char seq[32];
  size_t pos = 0;

  seq[pos++] = '\x1b';
  seq[pos++] = '[';
  pos += (size_t)lt__win_write_uint(seq + pos, y + 1);
  seq[pos++] = ';';
  pos += (size_t)lt__win_write_uint(seq + pos, x + 1);
  seq[pos++] = 'H';

  return lt__plat_write(seq, pos);
}

int lt__plat_hide_cursor(void) {
  static const char seq[] = "\x1b[?25l";
  return lt__plat_write(seq, sizeof(seq) - 1);
}

int lt__plat_show_cursor(void) {
  static const char seq[] = "\x1b[?25h";
  return lt__plat_write(seq, sizeof(seq) - 1);
}

int lt__plat_render_cell(int x, int y, const struct lt_cell *c) {
  (void)x;
  (void)y;

  lt_uchar ch = c->ch ? c->ch : (lt_uchar)' ';

  char utf8[4];
  int ub = lt__utf8_encode(ch, utf8);
  if (ub <= 0) {
    utf8[0] = ' ';
    ub = 1;
  }

  return lt__plat_write(utf8, (size_t)ub);
}
