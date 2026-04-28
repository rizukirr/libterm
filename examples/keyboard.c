#include "libterm/libterm.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  if (lt_init() != LT_OK)
    return 1;
  lt_clear();

  struct lt_event ev;
  int rc = lt_poll_event(&ev);
  (void)rc;
  assert(!(ev.type == LT_EVENT_KEY && ev.key != 0 && ev.ch != 0));

  const char *shape = "non-key";
  if (ev.type == LT_EVENT_KEY) {
    if (ev.key != 0)
      shape = "named-key";
    else if (ev.ch != 0)
      shape = "char-key";
    else
      shape = "empty-key";
  }

  fprintf(stderr, "type=%u key=%u ch=%u mod=%d shape=%s\n", ev.type, ev.key,
          ev.ch, ev.mod, shape);

  lt_shutdown();
  return 0;
}
