#include "../../internal.h"
#include "../../platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* TODO:
 *   WaitForSingleObject on STD_INPUT_HANDLE for timeout_ms
 *   ReadConsoleInputW in a loop, translate:
 *     KEY_EVENT            → LT_EVENT_KEY   (see plat_keys.c)
 *     MOUSE_EVENT          → LT_EVENT_MOUSE
 *     WINDOW_BUFFER_SIZE_EVENT → LT_EVENT_RESIZE
 */
int lt__plat_read_event(struct lt_event *ev, int timeout_ms) {
  (void)ev;
  (void)timeout_ms;
  return LT_ERR_NO_EVENT;
}
