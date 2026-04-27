#include "../../internal.h"
#include "../../platform.h"
#include "libterm/libterm.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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
      COORD sz = rec.Event.WindowBufferSizeEvent.dwSize;
      if (sz.X > 0 && sz.Y > 0) {
        int new_w = (int)sz.X;
        int new_h = (int)sz.Y;

        int rc = lt__buffer_resize(new_w, new_h);
        if (rc != LT_OK)
          return rc;

        memset(ev, 0, sizeof(struct lt_event));
        ev->type = LT_EVENT_RESIZE;
        ev->w = new_w;
        ev->h = new_h;
        return LT_OK;
      }
      continue;
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

    switch (kev.wVirtualKeyCode) {
    case VK_RETURN:
      ev->key = LT_KEY_ENTER;
      return LT_OK;
    case VK_ESCAPE:
      ev->key = LT_KEY_ESC;
      return LT_OK;
    case VK_BACK:
      ev->key = LT_KEY_BACKSPACE;
      return LT_OK;
    case VK_TAB:
      ev->key = LT_KEY_TAB;
      return LT_OK;
    case VK_UP:
      ev->key = LT_KEY_ARROW_UP;
      return LT_OK;
    case VK_DOWN:
      ev->key = LT_KEY_ARROW_DOWN;
      return LT_OK;
    case VK_LEFT:
      ev->key = LT_KEY_ARROW_LEFT;
      return LT_OK;
    case VK_RIGHT:
      ev->key = LT_KEY_ARROW_RIGHT;
      return LT_OK;
    default:
      break;
    }

    if (kev.uChar.UnicodeChar != 0) {
      ev->ch = (lt_uchar)kev.uChar.UnicodeChar;
      return LT_OK;
    }

    continue;
  }

  return LT_ERR_NO_EVENT;
}
