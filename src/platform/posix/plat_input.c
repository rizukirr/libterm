#include "../../internal.h"
#include "../../platform.h"

#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

static size_t lt__posix_read_esc_tail(unsigned char *buf, size_t cap) {
  size_t len = 1;

  while (len < cap) {
    struct timeval tv = {.tv_sec = 0, .tv_usec = 0};

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);

    int rc = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
    if (rc <= 0)
      break;

    ssize_t n = read(STDIN_FILENO, &buf[len], 1);
    if (n != 1)
      break;

    len++;
  }

  return len;
}

static int lt__posix_emit_char_event(struct lt_event *ev, unsigned char ch) {
  ev->type = LT_EVENT_KEY;
  ev->mod = 0;
  ev->key = 0;
  ev->ch = (lt_uchar)ch;
  return LT_OK;
}

int lt__plat_read_event(struct lt_event *ev, int timeout_ms) {
  if (!ev)
    return LT_ERR;

  while (true) {
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

    if (n > 0) {
      if (ch == '\x1b') {
        unsigned char seq[3] = {'\x1b', 0, 0};
        size_t seq_len = lt__posix_read_esc_tail(seq, sizeof(seq));

        ev->type = LT_EVENT_KEY;
        ev->mod = 0;
        ev->key = 0;

        if (seq_len == 3 && seq[1] == '[') {
          switch (seq[2]) {
          case 'A':
            ev->key = LT_KEY_ARROW_UP;
            break;

          case 'B':
            ev->key = LT_KEY_ARROW_DOWN;
            break;

          case 'C':
            ev->key = LT_KEY_ARROW_RIGHT;
            break;

          case 'D':
            ev->key = LT_KEY_ARROW_LEFT;
            break;
          default:
            break;
          }
        }

        if (seq_len == 1 ||
            (seq_len == 2 && (seq[1] == '\n' || seq[1] == '\r')))
          ev->key = LT_KEY_ESC;

        ev->ch = (ev->key == 0 && seq_len == 1) ? (lt_uchar)seq[0] : 0;
        return LT_OK;
      }

      return lt__posix_emit_char_event(ev, ch);
    }

    if (n == 0)
      return LT_ERR_NO_EVENT;

    if (errno == EINTR)
      continue;

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      if (timeout_ms < 0)
        continue;
      return LT_ERR_NO_EVENT;
    }

    return LT_ERR_READ;
  }
  return LT_OK;
}
