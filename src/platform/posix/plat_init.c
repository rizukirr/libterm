#include "../../internal.h"
#include "../../platform.h"

/* TODO: open /dev/tty, save termios, set raw mode, install SIGWINCH handler */
int lt__plat_init(void) { return LT_OK; }
int lt__plat_shutdown(void) { return LT_OK; }
int lt__plat_get_size(int *w, int *h) {
  *w = 80;
  *h = 24;
  return LT_OK;
}
