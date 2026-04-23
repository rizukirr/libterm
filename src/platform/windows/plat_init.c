#include "../../internal.h"
#include "../../platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* TODO:
 *   GetStdHandle(STD_INPUT_HANDLE / STD_OUTPUT_HANDLE)
 *   GetConsoleMode → save original
 *   SetConsoleMode: ENABLE_VIRTUAL_TERMINAL_PROCESSING for output,
 *                   ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT for input,
 *                   disable ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT |
 * ENABLE_PROCESSED_INPUT SetConsoleOutputCP(CP_UTF8)
 */
int lt__plat_init(void) { return LT_OK; }
int lt__plat_shutdown(void) { return LT_OK; }

int lt__plat_get_size(int *w, int *h) {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (!GetConsoleScreenBufferInfo(out, &csbi))
    return LT_ERR;
  *w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
  *h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
  return LT_OK;
}
