#include "libterm/libterm.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  if (lt_init() != LT_OK)
    return 1;
  for (;;) {
    struct lt_event ev;
    if (lt_poll_event(&ev) != LT_OK)
      continue;
    if (ev.type == LT_EVENT_KEY) {
      fprintf(stderr, "key=0x%04x ch=0x%08x mod=0x%02x\n", (unsigned)ev.key,
              (unsigned)ev.ch, (unsigned)ev.mod);
      if (ev.key == LT_KEY_ESC)
        break;
    }
  }
  lt_shutdown();
  return 0;
}
