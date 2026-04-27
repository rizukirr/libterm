#include "../../internal.h"
#include "../../platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HANDLE lt__win_in = INVALID_HANDLE_VALUE;
static HANDLE lt__win_out = INVALID_HANDLE_VALUE;
static DWORD lt__win_in_mode_orig = 0;
static DWORD lt__win_out_mode_orig = 0;
static int lt__win_modes_saved = 0;

static int lt__win_is_bad_handle(HANDLE h) {
  return h == NULL || h == INVALID_HANDLE_VALUE;
}

int lt__plat_init(void) {
  lt__win_in = GetStdHandle(STD_INPUT_HANDLE);
  lt__win_out = GetStdHandle(STD_OUTPUT_HANDLE);

  if (lt__win_is_bad_handle(lt__win_in) || lt__win_is_bad_handle(lt__win_out))
    return LT_ERR_INIT_OPEN;

  if (!GetConsoleMode(lt__win_in, &lt__win_in_mode_orig))
    return LT_ERR_INIT_OPEN;

  if (!GetConsoleMode(lt__win_out, &lt__win_out_mode_orig))
    return LT_ERR_INIT_OPEN;

  DWORD in_mode = lt__win_in_mode_orig;
  in_mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
  in_mode |= ENABLE_WINDOW_INPUT;

  if (!SetConsoleMode(lt__win_in, in_mode))
    return LT_ERR_INIT_OPEN;

  DWORD out_mode = lt__win_out_mode_orig;
  out_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  if (!SetConsoleMode(lt__win_out, out_mode)) {
    (void)SetConsoleMode(lt__win_in, lt__win_in_mode_orig);
    return LT_ERR_INIT_OPEN;
  }

  (void)SetConsoleCP(CP_UTF8);
  (void)SetConsoleOutputCP(CP_UTF8);

  lt__win_modes_saved = 1;
  return LT_OK;
}

int lt__plat_shutdown(void) {
  if (lt__win_modes_saved) {
    (void)SetConsoleMode(lt__win_in, lt__win_in_mode_orig);
    (void)SetConsoleMode(lt__win_out, lt__win_out_mode_orig);
  }

  lt__win_in = INVALID_HANDLE_VALUE;
  lt__win_out = INVALID_HANDLE_VALUE;
  lt__win_in_mode_orig = 0;
  lt__win_out_mode_orig = 0;
  lt__win_modes_saved = 0;
  return LT_OK;
}

int lt__plat_get_size(int *w, int *h) {
  if (!w || !h)
    return LT_ERR;

  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (!GetConsoleScreenBufferInfo(lt__win_out, &csbi))
    return LT_ERR;

  *w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
  *h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
  return LT_OK;
}
