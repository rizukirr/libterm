/* POSIX fragmented-input test: feeds escape sequences through an openpty()
 * master from a forked writer, with a real time gap between fragments, and
 * asserts the reader reassembles them instead of truncating at the gap.
 *
 * This is the regression guard for the esc-tail inter-byte timeout. The
 * all-bytes-at-once tests (test_posix_input_parse / _utf8) cannot catch the
 * bug because their bytes are always already buffered when select() runs; only
 * a separate writer process with a sleep between writes exercises the path
 * where the fd is momentarily empty mid-sequence.
 *
 * POSIX-only; returns 77 (CTest "skip") when no pty is available. */
#define _DEFAULT_SOURCE
#include "libterm/libterm.h"

#include <assert.h>
#if defined(__APPLE__)
#include <util.h> /* openpty lives here on macOS (no <pty.h>, no libutil) */
#else
#include <pty.h>
#endif
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void nap_ms(long ms) {
  struct timespec t = {ms / 1000, (ms % 1000) * 1000000L};
  nanosleep(&t, NULL);
}

/* Fork a writer that emits `bytes` split into two fragments around a gap. The
 * gap (40ms) exceeds zero but stays under the reader's grace period (50ms), so
 * a correct reader waits and reassembles while a zero-timeout reader truncates.
 * Returns the child pid. */
static pid_t spawn_fragmented_writer(int master, const unsigned char *bytes,
                                     size_t n, size_t split) {
  pid_t pid = fork();
  if (pid != 0)
    return pid; /* parent (or fork failure: caller's assert catches pid<0) */

  nap_ms(30); /* let the reader block in select() first */
  ssize_t w1 = write(master, bytes, split);
  (void)w1;
  nap_ms(40); /* inter-fragment gap */
  ssize_t w2 = write(master, bytes + split, n - split);
  (void)w2;
  nap_ms(100);
  _exit(0);
}

static void expect_event(int slave, int master, const unsigned char *bytes,
                         size_t n, size_t split, uint8_t want_type,
                         uint16_t want_key) {
  pid_t pid = spawn_fragmented_writer(master, bytes, n, split);
  assert(pid > 0);

  struct lt_event ev;
  memset(&ev, 0, sizeof(ev));
  int rc = lt_peek_event(&ev, 3000);
  assert(rc == LT_OK);
  assert(ev.type == want_type);
  assert(ev.key == want_key);

  int st;
  waitpid(pid, &st, 0);
}

int main(void) {
  int master = -1, slave = -1;
  struct winsize ws;
  memset(&ws, 0, sizeof ws);
  ws.ws_row = 24;
  ws.ws_col = 80;

  if (openpty(&master, &slave, NULL, NULL, &ws) != 0)
    return 77; /* CTest skip */

  assert(lt_init_fd(slave) == LT_OK);

  /* Arrow-up ESC [ A split after ESC: must reassemble to ARROW_UP, not a bare
   * ESC followed by a stray '[A'. This is the core regression. */
  {
    const unsigned char s[] = {'\x1b', '[', 'A'};
    expect_event(slave, master, s, sizeof s, 1, LT_EVENT_KEY, LT_KEY_ARROW_UP);
  }

  /* A longer CSI sequence (F5 = ESC [ 1 5 ~) split mid-parameters. */
  {
    const unsigned char s[] = {'\x1b', '[', '1', '5', '~'};
    expect_event(slave, master, s, sizeof s, 2, LT_EVENT_KEY, LT_KEY_F5);
  }

  /* SGR mouse report split mid-sequence still parses as a mouse event. */
  {
    const unsigned char s[] = {'\x1b', '[', '<', '0', ';', '5', ';', '7', 'M'};
    expect_event(slave, master, s, sizeof s, 3, LT_EVENT_MOUSE,
                 LT_KEY_MOUSE_LEFT);
  }

  /* Lone ESC with no trailing bytes must still resolve to LT_KEY_ESC after the
   * grace period (the disambiguation window must not hang or drop it). */
  {
    pid_t pid = fork();
    if (pid == 0) {
      nap_ms(30);
      write(master, "\x1b", 1);
      nap_ms(150);
      _exit(0);
    }
    assert(pid > 0);
    struct lt_event ev;
    memset(&ev, 0, sizeof(ev));
    int rc = lt_peek_event(&ev, 3000);
    assert(rc == LT_OK);
    assert(ev.type == LT_EVENT_KEY);
    assert(ev.key == LT_KEY_ESC);
    int st;
    waitpid(pid, &st, 0);
  }

  /* UTF-8 character split after the lead byte with a gap that exceeds the
   * escape-sequence window (50 ms): ssh from a Windows client delivers the
   * bytes of one keystroke this far apart. The continuation reader must wait
   * longer than the ESC disambiguation window -- a pending lead byte can never
   * be a complete key by itself -- instead of emitting U+FFFD plus a stray
   * byte. */
  {
    pid_t pid = fork();
    if (pid == 0) {
      nap_ms(30);
      write(master, "\xc3", 1); /* lead byte of U+00E4 */
      nap_ms(200);              /* > LT__ESC_SEQ_TIMEOUT_MS, < tail window */
      write(master, "\xa4", 1);
      nap_ms(100);
      _exit(0);
    }
    assert(pid > 0);
    struct lt_event ev;
    memset(&ev, 0, sizeof(ev));
    int rc = lt_peek_event(&ev, 3000);
    assert(rc == LT_OK);
    assert(ev.type == LT_EVENT_KEY);
    assert(ev.ch == 0xE4);
    /* and nothing left over: the continuation byte was consumed */
    memset(&ev, 0, sizeof(ev));
    rc = lt_peek_event(&ev, 100);
    assert(rc != LT_OK || ev.ch != 0xA4);
    int st;
    waitpid(pid, &st, 0);
  }

  assert(lt_shutdown() == LT_OK);
  close(slave);
  close(master);
  return 0;
}
