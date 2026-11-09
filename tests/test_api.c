#include "libterm/libterm.h"
#include <assert.h>
#include <string.h>

static void test_strerror(void) {
  /* every code returns a non-NULL, non-empty string */
  const int codes[] = {
      LT_OK,
      LT_ERR,
      LT_ERR_NEED_MORE,
      LT_ERR_INIT_ALREADY,
      LT_ERR_INIT_OPEN,
      LT_ERR_MEM,
      LT_ERR_NO_EVENT,
      LT_ERR_NO_TERM,
      LT_ERR_NOT_INIT,
      LT_ERR_OUT_OF_BOUNDS,
      LT_ERR_READ,
      LT_ERR_RESIZE_IOCTL,
      LT_ERR_RESIZE_PIPE,
      LT_ERR_RESIZE_SIGACTION,
      LT_ERR_POLL,
      LT_ERR_TCGETATTR,
      LT_ERR_TCSETATTR,
      LT_ERR_UNSUPPORTED_TERM,
      LT_ERR_RESIZE_WRITE,
      LT_ERR_RESIZE_POLL,
      LT_ERR_RESIZE_READ,
      LT_ERR_RESIZE_SSCANF,
      LT_ERR_CAP_COLLISION,
  };
  const int n = (int)(sizeof codes / sizeof codes[0]);
  for (int i = 0; i < n; i++) {
    const char *s = lt_strerror(codes[i]);
    assert(s != NULL);
    assert(s[0] != '\0');
  }
  /* known codes map to mutually distinct strings */
  for (int i = 0; i < n; i++)
    for (int j = i + 1; j < n; j++)
      assert(strcmp(lt_strerror(codes[i]), lt_strerror(codes[j])) != 0);
  /* an unknown code returns a non-NULL fallback */
  assert(lt_strerror(123) != NULL);
  assert(lt_strerror(123)[0] != '\0');
}

static void test_not_init(void) {
  /* width/height read as 0 before init (no error code; they return state) */
  assert(lt_width() == 0);
  assert(lt_height() == 0);
  /* set_clear_attrs must reject when not initialized, like its siblings */
  assert(lt_set_clear_attrs(LT_GREEN, LT_BLACK) == LT_ERR_NOT_INIT);
}

static void test_modes(void) {
  /* input modes: setting returns the new mode; CURRENT reads it back */
  const int in_modes[] = {LT_INPUT_ESC, LT_INPUT_ALT, LT_INPUT_MOUSE};
  for (int i = 0; i < (int)(sizeof in_modes / sizeof in_modes[0]); i++) {
    assert(lt_set_input_mode(in_modes[i]) == in_modes[i]);
    assert(lt_set_input_mode(LT_INPUT_CURRENT) == in_modes[i]);
  }
  /* output modes: same contract across every documented value */
  const int out_modes[] = {LT_OUTPUT_NORMAL, LT_OUTPUT_256, LT_OUTPUT_216,
                           LT_OUTPUT_GRAYSCALE, LT_OUTPUT_TRUECOLOR};
  for (int i = 0; i < (int)(sizeof out_modes / sizeof out_modes[0]); i++) {
    assert(lt_set_output_mode(out_modes[i]) == out_modes[i]);
    assert(lt_set_output_mode(LT_OUTPUT_CURRENT) == out_modes[i]);
  }
  /* setters are permissive: an out-of-range value is stored and returned as-is
   * (pinned behavior — there is no validation in the current API) */
  assert(lt_set_input_mode(999) == 999);
  assert(lt_set_input_mode(LT_INPUT_CURRENT) == 999);
  assert(lt_set_output_mode(999) == 999);
  assert(lt_set_output_mode(LT_OUTPUT_CURRENT) == 999);
}

int main(void) {
  test_strerror();
  test_not_init();

  struct lt_event ev;

  /* stable metadata */
  assert(strcmp(lt_version(), "0.1.3") == 0);
  assert(lt_strerror(LT_OK) != 0);
  assert(lt_strerror(LT_ERR_NOT_INIT) != 0);

  /* not initialized yet */
  assert(lt_present() == LT_ERR_NOT_INIT);
  assert(lt_clear() == LT_ERR_NOT_INIT);
  assert(lt_set_cell(0, 0, 'x', LT_DEFAULT, LT_DEFAULT) == LT_ERR_NOT_INIT);
  assert(lt_set_cursor(0, 0) == LT_ERR_NOT_INIT);
  assert(lt_hide_cursor() == LT_ERR_NOT_INIT);
  assert(lt_show_cursor() == LT_ERR_NOT_INIT);
  assert(lt_poll_event(&ev) == LT_ERR_NOT_INIT);
  assert(lt_peek_event(&ev, 0) == LT_ERR_NOT_INIT);
  assert(lt_shutdown() == LT_ERR_NOT_INIT);

  /* The functions added after the original API must honor the same pre-init
   * guard (their headers document "LT_ERR_NOT_INIT before lt_init"). These were
   * previously unasserted. */
  {
    struct lt_cell cell;
    const lt_uchar cl[] = {'e', 0x0301};
    assert(lt_get_cell(0, 0, &cell) == LT_ERR_NOT_INIT);
    assert(lt_print(0, 0, LT_DEFAULT, LT_DEFAULT, "x") == LT_ERR_NOT_INIT);
    assert(lt_print_ex(0, 0, LT_DEFAULT, LT_DEFAULT, NULL, "x") ==
           LT_ERR_NOT_INIT);
    assert(lt_printf(0, 0, LT_DEFAULT, LT_DEFAULT, "%d", 1) == LT_ERR_NOT_INIT);
    assert(lt_printf_ex(0, 0, LT_DEFAULT, LT_DEFAULT, NULL, "%d", 1) ==
           LT_ERR_NOT_INIT);
    assert(lt_send("x", 1) == LT_ERR_NOT_INIT);
    assert(lt_sendf("%d", 1) == LT_ERR_NOT_INIT);
    assert(lt_set_cell_ex(0, 0, cl, 2, LT_DEFAULT, LT_DEFAULT) ==
           LT_ERR_NOT_INIT);
    assert(lt_extend_cell(0, 0, 0x0301) == LT_ERR_NOT_INIT);
    assert(lt_invalidate() == LT_ERR_NOT_INIT);
    assert(lt_get_fds(NULL, NULL) == LT_ERR_NOT_INIT);

    /* Arg-validation must not depend on init order: a NULL/empty arg is still
     * rejected, and the documented arg error (LT_ERR) is independent of the
     * not-init guard ordering. lt_get_cell guards init first, so NULL out still
     * reads as NOT_INIT here — that's the documented precedence. */
    assert(lt_print(0, 0, LT_DEFAULT, LT_DEFAULT, NULL) == LT_ERR_NOT_INIT);
  }

  /* mode getters/setters are stateful API and should roundtrip */
  assert(lt_set_input_mode(LT_INPUT_CURRENT) == 0);
  assert(lt_set_input_mode(LT_INPUT_ESC) == LT_INPUT_ESC);
  assert(lt_set_input_mode(LT_INPUT_CURRENT) == LT_INPUT_ESC);

  assert(lt_set_output_mode(LT_OUTPUT_CURRENT) == 0);
  assert(lt_set_output_mode(LT_OUTPUT_256) == LT_OUTPUT_256);
  assert(lt_set_output_mode(LT_OUTPUT_CURRENT) == LT_OUTPUT_256);

  test_modes();

  /* lifecycle happy path (may be unavailable in non-tty runners) */
  int irc = lt_init();
  if (irc == LT_ERR_INIT_OPEN)
    return 0;
  assert(irc == LT_OK);
  assert(lt_init() == LT_ERR_INIT_ALREADY);

  assert(lt_width() > 0);
  assert(lt_height() > 0);

  /* cursor + clear + present path */
  assert(lt_hide_cursor() == LT_OK);
  assert(lt_show_cursor() == LT_OK);
  assert(lt_set_clear_attrs(LT_GREEN, LT_BLACK) == LT_OK);
  assert(lt_clear() == LT_OK);
  assert(lt_present() == LT_OK);

  /* set_cell bounds */
  assert(lt_set_cell(0, 0, 'A', LT_WHITE, LT_BLACK) == LT_OK);
  assert(lt_set_cell(lt_width() - 1, lt_height() - 1, 'Z', LT_WHITE,
                     LT_BLACK) == LT_OK);
  assert(lt_set_cell(-1, 0, 'x', LT_DEFAULT, LT_DEFAULT) ==
         LT_ERR_OUT_OF_BOUNDS);
  assert(lt_set_cell(0, -1, 'x', LT_DEFAULT, LT_DEFAULT) ==
         LT_ERR_OUT_OF_BOUNDS);
  assert(lt_set_cell(lt_width(), 0, 'x', LT_DEFAULT, LT_DEFAULT) ==
         LT_ERR_OUT_OF_BOUNDS);
  assert(lt_set_cell(0, lt_height(), 'x', LT_DEFAULT, LT_DEFAULT) ==
         LT_ERR_OUT_OF_BOUNDS);

  assert(lt_present() == LT_OK);

  /* non-blocking peek should either get an event or timeout */
  int prc = lt_peek_event(&ev, 0);
  assert(prc == LT_OK || prc == LT_ERR_NO_EVENT);

  assert(lt_shutdown() == LT_OK);

  /* after shutdown, API should reject operational calls */
  assert(lt_present() == LT_ERR_NOT_INIT);
  assert(lt_clear() == LT_ERR_NOT_INIT);
  assert(lt_set_cell(0, 0, 'x', LT_DEFAULT, LT_DEFAULT) == LT_ERR_NOT_INIT);

  return 0;
}
