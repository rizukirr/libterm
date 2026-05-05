#include "internal.h"
#include "platform.h"
#include "win_internal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static WCHAR lt__win_pending_high = 0;

static HANDLE lt__win_input_handle(void) {
  return GetStdHandle(STD_INPUT_HANDLE);
}

static DWORD lt__win_wait_ms_from_timeout(int timeout_ms) {
  if (timeout_ms < 0)
    return INFINITE;

  return (DWORD)timeout_ms;
}

int lt__plat_read_event(struct lt_event *ev, int timeout_ms) {
  if (!ev)
    return LT_ERR;

  HANDLE in = lt__win_input_handle();
  if (in == NULL || in == INVALID_HANDLE_VALUE)
    return LT_ERR_POLL;

  while (true) {
    DWORD wait_ms = lt__win_wait_ms_from_timeout(timeout_ms);
    DWORD wrc = WaitForSingleObject(in, wait_ms);

    if (wrc == WAIT_TIMEOUT)
      return LT_ERR_NO_EVENT;
    if (wrc != WAIT_OBJECT_0)
      return LT_ERR_POLL;

    INPUT_RECORD rec;
    DWORD nread = 0;
    if (!ReadConsoleInputW(in, &rec, 1, &nread))
      return LT_ERR_READ;
    if (nread == 0)
      return LT_ERR_NO_EVENT;

    if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT) {
      lt__win_pending_high = 0;

      int new_w = 0, new_h = 0;
      int rc = lt__plat_get_size(&new_w, &new_h);
      if (rc != LT_OK)
        return rc;

      if (new_w <= 0 || new_h <= 0)
        continue;

      if (new_w == lt__g.width && new_h == lt__g.height)
        continue;

      rc = lt__buffer_resize(new_w, new_h);
      if (rc != LT_OK)
        return rc;

      lt__g.cur_x = -1;
      lt__g.cur_y = -1;

      memset(ev, 0, sizeof(struct lt_event));
      ev->type = LT_EVENT_RESIZE;
      ev->w = new_w;
      ev->h = new_h;
      return LT_OK;
    }

    if (rec.EventType != KEY_EVENT)
      continue;

    KEY_EVENT_RECORD kev = rec.Event.KeyEvent;
    if (!kev.bKeyDown)
      continue;

    memset(ev, 0, sizeof(struct lt_event));
    ev->type = LT_EVENT_KEY;

    if (kev.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
      ev->mod |= LT_MOD_CTRL;

    if (kev.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
      ev->mod |= LT_MOD_ALT;

    if (kev.dwControlKeyState & (SHIFT_PRESSED))
      ev->mod |= LT_MOD_SHIFT;

    WORD mapped = lt__win_vk_to_lt_key(kev.wVirtualKeyCode);
    if (mapped != 0) {
      lt__win_pending_high = 0;
      ev->key = mapped;
      return LT_OK;
    }

    WCHAR ch16 = kev.uChar.UnicodeChar;
    if (ch16 >= 0xD800 && ch16 <= 0xDBFF) {
      lt__win_pending_high = ch16;
      continue;
    }

    if (ch16 >= 0xDC00 && ch16 <= 0xDFFF) {
      if (lt__win_pending_high == 0) {
        ev->ch = 0xFFFD;
        lt__win_pending_high = 0;
      } else {
        lt_uchar cp = (lt_uchar)((lt__win_pending_high - 0xD800) << 10);
        cp |= (lt_uchar)(ch16 - 0xDC00);
        cp += 0x10000;
        ev->ch = cp;
        lt__win_pending_high = 0;
      }

      return LT_OK;
    }

    if (ch16 != 0) {
      if (lt__win_pending_high != 0)
        lt__win_pending_high = 0;

      ev->ch = (lt_uchar)ch16;
      return LT_OK;
    }

    continue;
  }

  return LT_ERR_NO_EVENT;
}
