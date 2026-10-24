#include "libterm/libterm.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

/* Expose parser helpers from plat_input.c for unit testing. */
#define static
#define lt__plat_read_event lt__plat_read_event__impl_for_test
#include "../src/platform/posix/plat_input.c"
#undef lt__plat_read_event
#undef static

static void expect_seq(const unsigned char *seq, size_t len, uint16_t want_key,
                       uint8_t want_mod) {
  struct lt_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = LT_EVENT_KEY;

  int rc = lt__posix_parse_esc_seq(seq, len, &ev);
  assert(rc == LT_OK);
  assert(ev.key == want_key);
  assert(ev.mod == want_mod);
}

/* Parser must not invent a key mapping for malformed/unhandled sequences.
 * The parser never returns an error code; instead key stays 0. */
static void expect_no_key(const unsigned char *seq, size_t len) {
  struct lt_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = LT_EVENT_KEY;

  int rc = lt__posix_parse_esc_seq(seq, len, &ev);
  assert(rc == LT_OK);
  assert(ev.key == 0);
}

/* SGR mouse report: assert the full event (type/key/x/y/mod) parsed from a
 * raw \x1b[<...M|m byte sequence. */
static void expect_mouse(const unsigned char *seq, size_t len,
                         uint16_t want_key, int want_x, int want_y,
                         uint8_t want_mod) {
  struct lt_event ev;
  memset(&ev, 0, sizeof(ev));

  int rc = lt__posix_parse_esc_seq(seq, len, &ev);
  assert(rc == LT_OK);
  assert(ev.type == LT_EVENT_MOUSE);
  assert(ev.key == want_key);
  assert(ev.x == want_x);
  assert(ev.y == want_y);
  assert(ev.mod == want_mod);
}

/* A sequence the mouse parser must reject (malformed / not 1006). It should
 * leave type != LT_EVENT_MOUSE so the key paths can claim it. */
static void expect_not_mouse(const unsigned char *seq, size_t len) {
  struct lt_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = LT_EVENT_KEY;

  int rc = lt__posix_parse_esc_seq(seq, len, &ev);
  assert(rc == LT_OK);
  assert(ev.type != LT_EVENT_MOUSE);
}

static void run_mouse_sgr(void) {
  /* Left press at col 12, row 5 (1-based wire -> 0-based event). */
  {
    const unsigned char s[] = "\x1b[<0;12;5M";
    expect_mouse(s, sizeof(s) - 1, LT_KEY_MOUSE_LEFT, 11, 4, 0);
  }
  /* Lowercase 'm' is release regardless of button code. */
  {
    const unsigned char s[] = "\x1b[<0;12;5m";
    expect_mouse(s, sizeof(s) - 1, LT_KEY_MOUSE_RELEASE, 11, 4, 0);
  }
  /* Middle (1) and right (2) buttons. */
  {
    const unsigned char s[] = "\x1b[<1;3;4M";
    expect_mouse(s, sizeof(s) - 1, LT_KEY_MOUSE_MIDDLE, 2, 3, 0);
  }
  {
    const unsigned char s[] = "\x1b[<2;3;4M";
    expect_mouse(s, sizeof(s) - 1, LT_KEY_MOUSE_RIGHT, 2, 3, 0);
  }
  /* Wheel up (64) / down (65): bit 6 set promotes button 0/1 to wheel. */
  {
    const unsigned char s[] = "\x1b[<64;1;1M";
    expect_mouse(s, sizeof(s) - 1, LT_KEY_MOUSE_WHEEL_UP, 0, 0, 0);
  }
  {
    const unsigned char s[] = "\x1b[<65;1;1M";
    expect_mouse(s, sizeof(s) - 1, LT_KEY_MOUSE_WHEEL_DOWN, 0, 0, 0);
  }
  /* Ctrl+left: button 0 | ctrl bit 16 = 16. */
  {
    const unsigned char s[] = "\x1b[<16;7;8M";
    expect_mouse(s, sizeof(s) - 1, LT_KEY_MOUSE_LEFT, 6, 7, LT_MOD_CTRL);
  }
  /* Shift+Alt+left: 0 | 4 | 8 = 12. */
  {
    const unsigned char s[] = "\x1b[<12;7;8M";
    expect_mouse(s, sizeof(s) - 1, LT_KEY_MOUSE_LEFT, 6, 7,
                 LT_MOD_SHIFT | LT_MOD_ALT);
  }
  /* Drag (motion bit 32) with left held: 0 | 32 = 32. */
  {
    const unsigned char s[] = "\x1b[<32;2;2M";
    expect_mouse(s, sizeof(s) - 1, LT_KEY_MOUSE_LEFT, 1, 1, LT_MOD_MOTION);
  }
  /* Three-digit coordinates (the old 8-byte buffer could not hold these). */
  {
    const unsigned char s[] = "\x1b[<0;223;223M";
    expect_mouse(s, sizeof(s) - 1, LT_KEY_MOUSE_LEFT, 222, 222, 0);
  }

  /* Negatives: missing final, missing a field, non-1006 CSI, and the legacy
   * X10 'M' form must all be rejected by the mouse parser. */
  {
    const unsigned char s[] = "\x1b[<0;12;5"; /* no final M/m */
    expect_not_mouse(s, sizeof(s) - 1);
  }
  {
    const unsigned char s[] = "\x1b[<0;12M"; /* only two fields */
    expect_not_mouse(s, sizeof(s) - 1);
  }
  {
    const unsigned char s[] = "\x1b[A"; /* arrow-up, not a mouse report */
    expect_not_mouse(s, sizeof(s) - 1);
  }
  /* Overflow guard: a field with far more digits than INT_MAX must not trigger
   * signed-integer overflow (UB). It still parses as a mouse event (saturated),
   * which is fine — the point is that it must not invoke UB. */
  {
    const unsigned char s[] = "\x1b[<99999999999999999999;1;1M";
    struct lt_event ev;
    memset(&ev, 0, sizeof(ev));
    assert(lt__posix_parse_esc_seq(s, sizeof(s) - 1, &ev) == LT_OK);
    assert(ev.type == LT_EVENT_MOUSE);
  }
}

/* Back-tab (Shift+Tab): CSI Z. Ref: tmux input-keys.c:121 (KEYC_BTAB ->
 * "\033[Z"), kitty keyboard-protocol.rst. */
static void run_back_tab(void) {
  const unsigned char s[] = {'\x1b', '[', 'Z'};
  expect_seq(s, sizeof(s), LT_KEY_BACK_TAB, 0);
}

/* CSI-u (fixterms/kitty): ESC [ codepoint ; (1+mods) u. Asserts the codepoint
 * lands in ev.ch and the modifier bits decode. Ref:
 * external/kitty/docs/keyboard-protocol.rst. */
static void expect_csi_u(const unsigned char *seq, size_t len, lt_uchar want_ch,
                         uint8_t want_mod) {
  struct lt_event ev;
  memset(&ev, 0, sizeof(ev));
  int rc = lt__posix_parse_esc_seq(seq, len, &ev);
  assert(rc == LT_OK);
  assert(ev.type == LT_EVENT_KEY);
  assert(ev.ch == want_ch);
  assert(ev.mod == want_mod);
}

static void run_csi_u(void) {
  /* Plain 'a' (codepoint 97), no modifier field -> no mods. */
  {
    const unsigned char s[] = "\x1b[97u";
    expect_csi_u(s, sizeof(s) - 1, 97, 0);
  }
  /* Ctrl+a: modifiers = 1 + 4 = 5. */
  {
    const unsigned char s[] = "\x1b[97;5u";
    expect_csi_u(s, sizeof(s) - 1, 97, LT_MOD_CTRL);
  }
  /* Shift only: 1 + 1 = 2. */
  {
    const unsigned char s[] = "\x1b[97;2u";
    expect_csi_u(s, sizeof(s) - 1, 97, LT_MOD_SHIFT);
  }
  /* Ctrl+Shift: 1 + 0b101 = 6 (kitty spec's worked example). */
  {
    const unsigned char s[] = "\x1b[97;6u";
    expect_csi_u(s, sizeof(s) - 1, 97, LT_MOD_SHIFT | LT_MOD_CTRL);
  }
  /* Alt: 1 + 2 = 3. */
  {
    const unsigned char s[] = "\x1b[97;3u";
    expect_csi_u(s, sizeof(s) - 1, 97, LT_MOD_ALT);
  }
  /* High modifier bits we don't model (super = 8 -> field 9) decode to no
   * LT_MOD_* and must not corrupt the low bits: super+ctrl = 1+4+8 = 13. */
  {
    const unsigned char s[] = "\x1b[97;13u";
    expect_csi_u(s, sizeof(s) - 1, 97, LT_MOD_CTRL);
  }
  /* Multi-digit codepoint: U+20AC (8364) = euro. */
  {
    const unsigned char s[] = "\x1b[8364u";
    expect_csi_u(s, sizeof(s) - 1, 8364, 0);
  }

  /* Negatives: missing codepoint, and a second sub-field (not CSI-u shape). */
  {
    const unsigned char s[] = "\x1b[;5u"; /* no codepoint */
    expect_no_key(s, sizeof(s) - 1);
  }
  {
    const unsigned char s[] = "\x1b[97;5;3u"; /* extra ';' field */
    expect_no_key(s, sizeof(s) - 1);
  }
  /* Overflow guard: a digit run far past INT_MAX must not invoke signed-integer
   * overflow (UB). It still parses as a key event (codepoint saturated); the
   * point is that running this under UBSan must stay clean. Covers both the
   * codepoint field and the modifier field. */
  {
    const unsigned char s[] = "\x1b[99999999999u";
    struct lt_event ev;
    memset(&ev, 0, sizeof(ev));
    assert(lt__posix_parse_esc_seq(s, sizeof(s) - 1, &ev) == LT_OK);
    assert(ev.type == LT_EVENT_KEY);
  }
  {
    const unsigned char s[] = "\x1b[1;99999999999u";
    struct lt_event ev;
    memset(&ev, 0, sizeof(ev));
    assert(lt__posix_parse_esc_seq(s, sizeof(s) - 1, &ev) == LT_OK);
  }
}

/* Overflow guard for the other two CSI numeric parsers (tilde-key code/mod and
 * letter-final modifier). Long digit runs must not overflow; UBSan-clean. */
static void run_csi_overflow(void) {
  struct lt_event ev;
  /* tilde: huge code and huge modifier field. */
  {
    const unsigned char s[] = "\x1b[99999999999;99999999999~";
    memset(&ev, 0, sizeof(ev));
    assert(lt__posix_parse_esc_seq(s, sizeof(s) - 1, &ev) == LT_OK);
  }
  /* letter-final: huge modifier on an arrow key. */
  {
    const unsigned char s[] = "\x1b[1;99999999999A";
    memset(&ev, 0, sizeof(ev));
    assert(lt__posix_parse_esc_seq(s, sizeof(s) - 1, &ev) == LT_OK);
  }
  /* mouse SGR: huge button/coord fields. */
  {
    const unsigned char s[] = "\x1b[<99999999999;99999999999;99999999999M";
    memset(&ev, 0, sizeof(ev));
    assert(lt__posix_parse_esc_seq(s, sizeof(s) - 1, &ev) == LT_OK);
  }
}

struct mod_case {
  unsigned char digit;
  uint8_t want_mod;
};

struct letter_case {
  unsigned char final;
  uint16_t want_key;
};

static void run_letter_mod_matrix(void) {
  const struct mod_case mods[] = {
      {'2', LT_MOD_SHIFT},
      {'3', LT_MOD_ALT},
      {'4', LT_MOD_SHIFT | LT_MOD_ALT},
      {'5', LT_MOD_CTRL},
      {'6', LT_MOD_SHIFT | LT_MOD_CTRL},
      {'7', LT_MOD_ALT | LT_MOD_CTRL},
      {'8', LT_MOD_SHIFT | LT_MOD_ALT | LT_MOD_CTRL},
  };
  const struct letter_case keys[] = {
      {'A', LT_KEY_ARROW_UP},    {'B', LT_KEY_ARROW_DOWN},
      {'C', LT_KEY_ARROW_RIGHT}, {'D', LT_KEY_ARROW_LEFT},
      {'H', LT_KEY_HOME},        {'F', LT_KEY_END},
  };

  for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
    for (size_t m = 0; m < sizeof(mods) / sizeof(mods[0]); m++) {
      const unsigned char s[] = {'\x1b', '[',           '1',
                                 ';',    mods[m].digit, keys[k].final};
      expect_seq(s, sizeof(s), keys[k].want_key, mods[m].want_mod);
    }
  }
}

/* F1..F4 WITH modifiers: xterm and Windows Terminal send CSI 1;<mod> P..S
 * (e.g. ESC [ 1 ; 5 P = Ctrl-F1), unlike F5..F12 (CSI <n>;<mod> ~). The
 * CSI-letter branch only knew A B C D H F, so these fell through as an
 * empty event. */
static void run_csi_letter_f1_f4_mods(void) {
  const struct mod_case mods[] = {
      {'2', LT_MOD_SHIFT},
      {'3', LT_MOD_ALT},
      {'5', LT_MOD_CTRL},
      {'6', LT_MOD_SHIFT | LT_MOD_CTRL},
      {'8', LT_MOD_SHIFT | LT_MOD_ALT | LT_MOD_CTRL},
  };
  const struct letter_case keys[] = {
      {'P', LT_KEY_F1},
      {'Q', LT_KEY_F2},
      {'R', LT_KEY_F3},
      {'S', LT_KEY_F4},
  };

  for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
    for (size_t m = 0; m < sizeof(mods) / sizeof(mods[0]); m++) {
      const unsigned char s[] = {'\x1b', '[',           '1',
                                 ';',    mods[m].digit, keys[k].final};
      expect_seq(s, sizeof(s), keys[k].want_key, mods[m].want_mod);
    }
  }
}

static void run_bare_letter_keys(void) {
  /* No modifier — ESC [ <final>. */
  const struct letter_case keys[] = {
      {'A', LT_KEY_ARROW_UP},    {'B', LT_KEY_ARROW_DOWN},
      {'C', LT_KEY_ARROW_RIGHT}, {'D', LT_KEY_ARROW_LEFT},
      {'H', LT_KEY_HOME},        {'F', LT_KEY_END},
  };

  for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
    const unsigned char s[] = {'\x1b', '[', keys[k].final};
    expect_seq(s, sizeof(s), keys[k].want_key, 0);
  }
}

static void run_ss3_keys(void) {
  const struct letter_case keys[] = {
      {'H', LT_KEY_HOME}, {'F', LT_KEY_END}, {'P', LT_KEY_F1},
      {'Q', LT_KEY_F2},   {'R', LT_KEY_F3},  {'S', LT_KEY_F4},
  };

  for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
    const unsigned char s[] = {'\x1b', 'O', keys[k].final};
    expect_seq(s, sizeof(s), keys[k].want_key, 0);
  }
}

static void run_tilde_happy(void) {
  /* Tilde-final without modifier. */
  {
    const unsigned char s[] = {'\x1b', '[', '2', '~'};
    expect_seq(s, sizeof(s), LT_KEY_INSERT, 0);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '3', '~'};
    expect_seq(s, sizeof(s), LT_KEY_DELETE, 0);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '5', '~'};
    expect_seq(s, sizeof(s), LT_KEY_PGUP, 0);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '6', '~'};
    expect_seq(s, sizeof(s), LT_KEY_PGDN, 0);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '1', '5', '~'};
    expect_seq(s, sizeof(s), LT_KEY_F5, 0);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '2', '4', '~'};
    expect_seq(s, sizeof(s), LT_KEY_F12, 0);
  }
}

static void run_tilde_with_mod(void) {
  /* Representative tilde + modifier samples. */
  {
    const unsigned char s[] = {'\x1b', '[', '3', ';', '5', '~'};
    expect_seq(s, sizeof(s), LT_KEY_DELETE, LT_MOD_CTRL);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '5', ';', '2', '~'};
    expect_seq(s, sizeof(s), LT_KEY_PGUP, LT_MOD_SHIFT);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '1', '5', ';', '8', '~'};
    expect_seq(s, sizeof(s), LT_KEY_F5,
               LT_MOD_SHIFT | LT_MOD_ALT | LT_MOD_CTRL);
  }
}

static void run_bare_esc(void) {
  /* Lone ESC, and ESC followed by a newline/CR, both map to LT_KEY_ESC. */
  {
    const unsigned char s[] = {'\x1b'};
    expect_seq(s, sizeof(s), LT_KEY_ESC, 0);
  }
  {
    const unsigned char s[] = {'\x1b', '\n'};
    expect_seq(s, sizeof(s), LT_KEY_ESC, 0);
  }
  {
    const unsigned char s[] = {'\x1b', '\r'};
    expect_seq(s, sizeof(s), LT_KEY_ESC, 0);
  }
}

static void run_negative_cases(void) {
  /* Truncated CSI: ESC [ alone — too short for any branch, no key. */
  {
    const unsigned char s[] = {'\x1b', '['};
    expect_no_key(s, sizeof(s));
  }
  /* CSI Z is back-tab (Shift+Tab) — now mapped (see run_back_tab below). */
  {
    const unsigned char s[] = {'\x1b', '[', 'Z'};
    expect_seq(s, sizeof(s), LT_KEY_BACK_TAB, 0);
  }
  /* Garbage CSI final. */
  {
    const unsigned char s[] = {'\x1b', '[', 'Q'};
    expect_no_key(s, sizeof(s));
  }
  /* Unknown SS3 final. */
  {
    const unsigned char s[] = {'\x1b', 'O', 'Z'};
    expect_no_key(s, sizeof(s));
  }
  /* Oversized but well-formed tilde code that maps to no key. */
  {
    const unsigned char s[] = {'\x1b', '[', '9', '9', '9', '~'};
    expect_no_key(s, sizeof(s));
  }
  /* Tilde with non-digit in code resets to 0 → no mapping. */
  {
    const unsigned char s[] = {'\x1b', '[', '1', 'X', '~'};
    expect_no_key(s, sizeof(s));
  }
  /* Bare ESC O — too short for SS3 lookup. */
  {
    const unsigned char s[] = {'\x1b', 'O'};
    expect_no_key(s, sizeof(s));
  }
  /* Empty modifier slot: ESC [ 1 ; A — letter-mod helper returns 0
   * (non-digit in mod position), so mod stays 0 but the letter still
   * resolves. This pins current behavior. */
  {
    const unsigned char s[] = {'\x1b', '[', '1', ';', 'A'};
    expect_seq(s, sizeof(s), LT_KEY_ARROW_UP, 0);
  }
  /* Non-CSI / non-SS3 second byte. */
  {
    const unsigned char s[] = {'\x1b', 'X', 'Y'};
    expect_no_key(s, sizeof(s));
  }
}

static void expect_ctrl_byte(unsigned char b, uint16_t want_key) {
  struct lt_event ev;
  memset(&ev, 0, sizeof(ev));
  bool consumed = lt__posix_ctrl_byte_event(b, &ev);
  assert(consumed);
  assert(ev.type == LT_EVENT_KEY);
  assert(ev.key == want_key);
  assert(ev.ch == 0);
  assert(ev.mod == 0);
}

static void expect_not_ctrl_byte(unsigned char b) {
  struct lt_event ev;
  memset(&ev, 0, sizeof(ev));
  assert(!lt__posix_ctrl_byte_event(b, &ev));
}

static void run_ctrl_byte_keys(void) {
  /* Ctrl+letter control bytes map to LT_KEY_CTRL_* (== the byte), ch == 0. */
  expect_ctrl_byte(0x01, LT_KEY_CTRL_A);
  expect_ctrl_byte(0x03, LT_KEY_CTRL_C);
  expect_ctrl_byte(0x1A, LT_KEY_CTRL_Z);

  /* Named keys that share control-byte values resolve to the named code. */
  expect_ctrl_byte(0x08, LT_KEY_BACKSPACE); /* == CTRL_H */
  expect_ctrl_byte(0x09, LT_KEY_TAB);       /* == CTRL_I */
  expect_ctrl_byte(0x0D, LT_KEY_ENTER);     /* == CTRL_M */

  /* Upper control range and DEL. */
  expect_ctrl_byte(0x1C, LT_KEY_CTRL_4);
  expect_ctrl_byte(0x1F, LT_KEY_CTRL_7);
  expect_ctrl_byte(0x7F, LT_KEY_BACKSPACE2); /* == CTRL_8 */

  /* Printable bytes are NOT control bytes — they go through the UTF-8 path. */
  expect_not_ctrl_byte(0x20); /* space */
  expect_not_ctrl_byte('A');
  expect_not_ctrl_byte(0x7E); /* '~' */
}

static void run_input_mode_alt(void) {
  /* LT_INPUT_ALT: Alt+<key> is a single event with LT_MOD_ALT. */
  lt__g.input_mode = LT_INPUT_ALT;

  /* Alt+a -> ch='a', mod=ALT (printable). */
  {
    struct lt_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = LT_EVENT_KEY;
    const unsigned char s[] = {'\x1b', 'a'};
    assert(lt__posix_handle_alt_combo(s, sizeof(s), &ev) == LT_OK);
    assert(ev.key == 0);
    assert(ev.ch == 'a');
    assert(ev.mod == LT_MOD_ALT);
  }

  /* Alt+Ctrl+a (ESC 0x01) -> key=CTRL_A, ch=0, mod=ALT (control byte). */
  {
    struct lt_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = LT_EVENT_KEY;
    const unsigned char s[] = {'\x1b', 0x01};
    assert(lt__posix_handle_alt_combo(s, sizeof(s), &ev) == LT_OK);
    assert(ev.key == LT_KEY_CTRL_A);
    assert(ev.ch == 0);
    assert(ev.mod == LT_MOD_ALT);
  }
}

static void run_input_mode_modern(void) {
  /* Modern model (no LT_INPUT_COMPAT, the default): Alt+<key> folds into one
   * event with LT_MOD_ALT — the ESC/ALT distinction is moot here. */
  lt__g.input_mode = 0;

  struct lt_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = LT_EVENT_KEY;
  const unsigned char s[] = {'\x1b', 'a'};
  assert(lt__posix_handle_alt_combo(s, sizeof(s), &ev) == LT_OK);
  assert(ev.ch == 'a');
  assert(ev.key == 0);
  assert(ev.mod == LT_MOD_ALT);
}

static void run_input_mode_esc(void) {
  /* Compat mode defaults to the termbox2 ESC behavior: Alt+<key> -> LT_KEY_ESC
   * now, then the byte replayed as the next event (two-event default). */
  lt__g.input_mode = LT_INPUT_COMPAT;
  lt__posix_pending_len = 0;
  lt__posix_pending_pos = 0;

  struct lt_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = LT_EVENT_KEY;
  const unsigned char s[] = {'\x1b', 'a'};
  assert(lt__posix_handle_alt_combo(s, sizeof(s), &ev) == LT_OK);
  assert(ev.key == LT_KEY_ESC);
  assert(ev.ch == 0);
  assert(ev.mod == 0);

  /* The 'a' is now pending — pop it as the following event. */
  struct lt_event ev2;
  memset(&ev2, 0, sizeof(ev2));
  assert(lt__posix_pending_pop(&ev2));
  assert(ev2.ch == 'a');
  assert(ev2.key == 0);

  /* Buffer is drained afterwards. */
  struct lt_event ev3;
  memset(&ev3, 0, sizeof(ev3));
  assert(!lt__posix_pending_pop(&ev3));
}

int main(void) {
  run_letter_mod_matrix();
  run_csi_letter_f1_f4_mods();
  run_bare_letter_keys();
  run_ss3_keys();
  run_tilde_happy();
  run_tilde_with_mod();
  run_bare_esc();
  run_negative_cases();
  run_ctrl_byte_keys();
  run_input_mode_alt();
  run_input_mode_modern();
  run_input_mode_esc();
  run_mouse_sgr();
  run_back_tab();
  run_csi_u();
  run_csi_overflow();
  return 0;
}
