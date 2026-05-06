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

static char *lt__plat_reserve(size_t max) {
  if (max > LT__OUTBUF_CAP)
    return NULL;

  if (lt__outbuf_len + max > LT__OUTBUF_CAP) {
    if (lt__plat_flush() != LT_OK)
      return NULL;
  }

  return lt__outbuf + lt__outbuf_len;
}

static void lt__plat_commit(size_t actual) { lt__outbuf_len += actual; }

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

  /* worst case: '\x1b[' (2) + 10 digits + ';' (1) + 10 digits + 'H' (1) = 24 */
  char *p = lt__plat_reserve(24);
  if (!p)
    return LT_ERR;

  size_t pos = 0;
  p[pos++] = '\x1b';
  p[pos++] = '[';
  pos += (size_t)lt__win_write_uint(p + pos, y + 1);
  p[pos++] = ';';
  pos += (size_t)lt__win_write_uint(p + pos, x + 1);
  p[pos++] = 'H';

  lt__plat_commit(pos);
  return LT_OK;
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

  if (ch < 0x80) {
    char b = (char)ch;
    return lt__plat_write(&b, 1);
  }

  char utf8[4];
  int ub = lt__utf8_encode(ch, utf8);
  if (ub <= 0) {
    utf8[0] = ' ';
    ub = 1;
  }

  return lt__plat_write(utf8, (size_t)ub);
}

int lt__plat_render_run(const struct lt_cell *cells, int count) {
  if (count <= 0)
    return LT_OK;

  /* worst case: 4 UTF-8 bytes per cell */
  size_t max = (size_t)count * 4;
  char *p = lt__plat_reserve(max);
  if (!p) {
    /* run too big for outbuf or flush failed; fall back to per-cell write */
    int rc = 0;
    for (int i = 0; i < count; i++) {
      rc = lt__plat_render_cell(0, 0, &cells[i]);
      if (rc != LT_OK)
        return rc;
    }

    return LT_OK;
  }

  size_t pos = 0;
  lt_uchar ch = (lt_uchar)' ';
  int ub = 0;

  for (int i = 0; i < count; i++) {
    ch = cells[i].ch ? cells[i].ch : (lt_uchar)' ';

    if (ch < 0x80) {
      p[pos++] = (char)ch;
      continue;
    }

    char tmp[4];
    ub = lt__utf8_encode(ch, tmp);
    if (ub <= 0) {
      p[pos++] = ' ';
      continue;
    }

    for (int j = 0; j < ub; j++)
      p[pos++] = tmp[j];
  }

  lt__plat_commit(pos);
  return LT_OK;
}
