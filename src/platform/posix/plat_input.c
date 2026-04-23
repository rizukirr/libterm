#include "../../internal.h"
#include "../../platform.h"

#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

int lt__plat_read_event(struct lt_event *ev, int timeout_ms) {
  if (!ev)
    return LT_ERR;

  memset(ev, 0, sizeof(*ev));

  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(STDIN_FILENO, &rfds);

  struct timeval tv;
  struct timeval *tv_ptr = NULL;

  if (timeout_ms >= 0) {
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    tv_ptr = &tv;
  }

  int rc = select(STDIN_FILENO + 1, &rfds, NULL, NULL, tv_ptr);
  if (rc == 0)
    return LT_ERR_NO_EVENT;

  if (rc < 0) {
    if (errno == EINTR)
      return LT_ERR_NO_EVENT;
    return LT_ERR_POLL;
  }

  unsigned char ch = 0;
  ssize_t n = read(STDIN_FILENO, &ch, 1);
  if (n == 0)
    return LT_ERR_NO_EVENT;

  if (n < 0)
    return LT_ERR_READ;

  ev->type = LT_EVENT_KEY;
  ev->mod = 0;
  ev->key = 0;
  ev->ch = (lt_uchar)ch;
  return LT_OK;
}
