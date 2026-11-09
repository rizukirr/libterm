/*
 * libterm - pure key-sequence decoder.
 *
 * Interprets bytes already read from the terminal (by the platform input
 * layer) into an lt_event. No globals, no syscalls: the mirror of
 * lt__utf8_decode on the input side, so it can be unit-tested without a tty
 * and reused by any platform backend.
 */
#include "internal.h"
#include "libterm/libterm.h"
#include <string.h>

struct lt__key_seq {
  const char *bytes;
  uint16_t key;
  uint8_t mod;
};

/* Fixed, non-parametric escape sequences. Longest entries are fine in any
 * order; the matcher checks full equality and separately tracks prefixes. */
static const struct lt__key_seq lt__fixed_seqs[] = {
    /* xterm normal cursor / edit */
    {"\x1b[A", LT_KEY_ARROW_UP, 0},
    {"\x1b[B", LT_KEY_ARROW_DOWN, 0},
    {"\x1b[C", LT_KEY_ARROW_RIGHT, 0},
    {"\x1b[D", LT_KEY_ARROW_LEFT, 0},
    {"\x1b[H", LT_KEY_HOME, 0},
    {"\x1b[F", LT_KEY_END, 0},
    {"\x1b[Z", LT_KEY_BACK_TAB, 0},
    /* SS3 application cursor / Home / End */
    {"\x1bOA", LT_KEY_ARROW_UP, 0},
    {"\x1bOB", LT_KEY_ARROW_DOWN, 0},
    {"\x1bOC", LT_KEY_ARROW_RIGHT, 0},
    {"\x1bOD", LT_KEY_ARROW_LEFT, 0},
    {"\x1bOH", LT_KEY_HOME, 0},
    {"\x1bOF", LT_KEY_END, 0},
    /* SS3 function keys F1-F4 (xterm, screen, PuTTY) */
    {"\x1bOP", LT_KEY_F1, 0},
    {"\x1bOQ", LT_KEY_F2, 0},
    {"\x1bOR", LT_KEY_F3, 0},
    {"\x1bOS", LT_KEY_F4, 0},
    /* rxvt Shift+arrows (lowercase CSI finals) */
    {"\x1b[a", LT_KEY_ARROW_UP, LT_MOD_SHIFT},
    {"\x1b[b", LT_KEY_ARROW_DOWN, LT_MOD_SHIFT},
    {"\x1b[c", LT_KEY_ARROW_RIGHT, LT_MOD_SHIFT},
    {"\x1b[d", LT_KEY_ARROW_LEFT, LT_MOD_SHIFT},
    /* rxvt Ctrl+arrows (SS3 lowercase) */
    {"\x1bOa", LT_KEY_ARROW_UP, LT_MOD_CTRL},
    {"\x1bOb", LT_KEY_ARROW_DOWN, LT_MOD_CTRL},
    {"\x1bOc", LT_KEY_ARROW_RIGHT, LT_MOD_CTRL},
    {"\x1bOd", LT_KEY_ARROW_LEFT, LT_MOD_CTRL},
    /* Linux console F1-F5 */
    {"\x1b[[A", LT_KEY_F1, 0},
    {"\x1b[[B", LT_KEY_F2, 0},
    {"\x1b[[C", LT_KEY_F3, 0},
    {"\x1b[[D", LT_KEY_F4, 0},
    {"\x1b[[E", LT_KEY_F5, 0},
};

/* Try the fixed table. Returns MATCH (out filled, *consumed set), PARTIAL (seq
 * is a strict prefix of some entry), or NOMATCH. */
static enum lt__key_match lt__match_fixed(const unsigned char *seq, size_t len,
                                          struct lt_event *out,
                                          size_t *consumed) {
  bool any_prefix = false;
  for (size_t i = 0; i < sizeof(lt__fixed_seqs) / sizeof(lt__fixed_seqs[0]);
       i++) {
    const char *p = lt__fixed_seqs[i].bytes;
    size_t plen = strlen(p);
    if (len == plen && memcmp(seq, p, plen) == 0) {
      out->type = LT_EVENT_KEY;
      out->key = lt__fixed_seqs[i].key;
      out->mod = lt__fixed_seqs[i].mod;
      out->ch = 0;
      out->action = LT_KEY_PRESS;
      *consumed = len;
      return LT__KEY_MATCH;
    }
    if (len < plen && memcmp(seq, p, len) == 0)
      any_prefix = true;
  }
  return any_prefix ? LT__KEY_PARTIAL : LT__KEY_NOMATCH;
}

#define LT__KM_NUM_CAP 1000000
static int lt__km_accum(int acc, unsigned char d) {
  if (acc < LT__KM_NUM_CAP)
    return acc * 10 + (d - '0');
  return acc;
}

/* mods field is 1 + bitmask (shift=1, alt=2, ctrl=4, ...). */
static uint8_t lt__km_mods(int mods_field) {
  uint8_t m = 0;
  if (mods_field > 0) {
    int bits = mods_field - 1;
    if (bits & 1)
      m |= LT_MOD_SHIFT;
    if (bits & 2)
      m |= LT_MOD_ALT;
    if (bits & 4)
      m |= LT_MOD_CTRL;
  }
  return m;
}

static uint16_t lt__km_tilde_key(int code) {
  switch (code) {
  case 1:
  case 7:
    return LT_KEY_HOME;
  case 2:
    return LT_KEY_INSERT;
  case 3:
    return LT_KEY_DELETE;
  case 4:
  case 8:
    return LT_KEY_END;
  case 5:
    return LT_KEY_PGUP;
  case 6:
    return LT_KEY_PGDN;
  case 11:
    return LT_KEY_F1;
  case 12:
    return LT_KEY_F2;
  case 13:
    return LT_KEY_F3;
  case 14:
    return LT_KEY_F4;
  case 15:
    return LT_KEY_F5;
  case 17:
    return LT_KEY_F6;
  case 18:
    return LT_KEY_F7;
  case 19:
    return LT_KEY_F8;
  case 20:
    return LT_KEY_F9;
  case 21:
    return LT_KEY_F10;
  case 23:
    return LT_KEY_F11;
  case 24:
    return LT_KEY_F12;
  default:
    return 0;
  }
}

static uint16_t lt__km_letter_key(unsigned char final) {
  switch (final) {
  case 'A':
    return LT_KEY_ARROW_UP;
  case 'B':
    return LT_KEY_ARROW_DOWN;
  case 'C':
    return LT_KEY_ARROW_RIGHT;
  case 'D':
    return LT_KEY_ARROW_LEFT;
  case 'H':
    return LT_KEY_HOME;
  case 'F':
    return LT_KEY_END;
  case 'P':
    return LT_KEY_F1;
  case 'Q':
    return LT_KEY_F2;
  case 'R':
    return LT_KEY_F3;
  case 'S':
    return LT_KEY_F4;
  default:
    return 0;
  }
}

/* Parse ESC [ <p0> [; <p1>] <final> where finals are ~ (edit/function keys
 * by number), or A/B/C/D/H/F (cursor keys) or P/Q/R/S (F1-F4, the SS3 letters
 * move into a CSI when a modifier is held) and the params are decimal.
 * Returns MATCH/PARTIAL/NOMATCH. Only called for seq[1]=='[' with at least
 * one digit. */
static enum lt__key_match lt__match_csi_param(const unsigned char *seq,
                                              size_t len, struct lt_event *out,
                                              size_t *consumed) {
  int params[2] = {0, 0};
  int pi = 0;
  size_t i = 2;
  for (; i < len; i++) {
    unsigned char c = seq[i];
    if (c >= '0' && c <= '9') {
      params[pi <= 1 ? pi : 1] = lt__km_accum(params[pi <= 1 ? pi : 1], c);
    } else if (c == ';') {
      if (pi < 1)
        pi++;
    } else {
      break; /* final byte */
    }
  }
  if (i == len)
    return LT__KEY_PARTIAL; /* still collecting params, no final yet */

  unsigned char final = seq[i];
  if (i + 1 != len)
    return LT__KEY_NOMATCH; /* trailing junk after the final */

  uint16_t key = 0;
  if (final == '~')
    key = lt__km_tilde_key(params[0]);
  else
    key = lt__km_letter_key(final);
  if (key == 0)
    return LT__KEY_NOMATCH;

  out->type = LT_EVENT_KEY;
  out->key = key;
  out->mod = lt__km_mods(params[1]);
  out->ch = 0;
  out->action = LT_KEY_PRESS;
  *consumed = len;
  return LT__KEY_MATCH;
}

/* SGR (1006) mouse: ESC [ < Cb ; Cx ; Cy (M|m). Returns MATCH/PARTIAL/NOMATCH.
 * Only called when seq starts with ESC [ <. */
static enum lt__key_match lt__match_mouse(const unsigned char *seq, size_t len,
                                          struct lt_event *out,
                                          size_t *consumed) {
  int num[3] = {0, 0, 0};
  int ni = 0;
  bool seen = false;
  size_t i = 3;
  for (; i < len; i++) {
    unsigned char c = seq[i];
    if (c >= '0' && c <= '9') {
      num[ni] = lt__km_accum(num[ni], c);
      seen = true;
    } else if (c == ';') {
      if (!seen || ni >= 2)
        return LT__KEY_NOMATCH;
      ni++;
      seen = false;
    } else if (c == 'M' || c == 'm') {
      break;
    } else {
      return LT__KEY_NOMATCH;
    }
  }
  if (i == len)
    return LT__KEY_PARTIAL; /* no final M/m yet */
  if (ni != 2 || !seen || i + 1 != len)
    return LT__KEY_NOMATCH;

  unsigned char final = seq[i];
  int cb = num[0];
  switch (cb & 3) {
  case 0:
    out->key = (cb & 64) ? LT_KEY_MOUSE_WHEEL_UP : LT_KEY_MOUSE_LEFT;
    break;
  case 1:
    out->key = (cb & 64) ? LT_KEY_MOUSE_WHEEL_DOWN : LT_KEY_MOUSE_MIDDLE;
    break;
  case 2:
    out->key = LT_KEY_MOUSE_RIGHT;
    break;
  default:
    out->key = LT_KEY_MOUSE_RELEASE;
    break;
  }
  if (final == 'm')
    out->key = LT_KEY_MOUSE_RELEASE;

  out->mod = 0;
  if (cb & 4)
    out->mod |= LT_MOD_SHIFT;
  if (cb & 8)
    out->mod |= LT_MOD_ALT;
  if (cb & 16)
    out->mod |= LT_MOD_CTRL;
  if (cb & 32)
    out->mod |= LT_MOD_MOTION;

  out->x = num[1] > 0 ? num[1] - 1 : 0;
  out->y = num[2] > 0 ? num[2] - 1 : 0;
  out->type = LT_EVENT_MOUSE;
  out->action = LT_KEY_PRESS;
  *consumed = len;
  return LT__KEY_MATCH;
}

/* Map a kitty functional key code to an LT_KEY_*; 0 if it is a normal
 * codepoint that should be reported via ch. Only the bare-modifier codes and a
 * few are mapped here; functional navigation keys are reported through the
 * legacy tables since terminals also send those forms. */
static uint16_t lt__km_kitty_functional(int code) {
  switch (code) {
  case 57441:
    return LT_KEY_LEFT_SHIFT;
  case 57447:
    return LT_KEY_RIGHT_SHIFT;
  case 57442:
    return LT_KEY_LEFT_CTRL;
  case 57448:
    return LT_KEY_RIGHT_CTRL;
  case 57443:
    return LT_KEY_LEFT_ALT;
  case 57449:
    return LT_KEY_RIGHT_ALT;
  case 57444:
    return LT_KEY_LEFT_SUPER;
  case 57450:
    return LT_KEY_RIGHT_SUPER;
  case 57358:
    return LT_KEY_CAPS_LOCK;
  default:
    return 0;
  }
}

/* Named key for a control codepoint, or 0 if none. Shared by the control-byte
 * path and the kitty CSI-u path so the same physical key yields the same event
 * regardless of how the terminal encoded it (raw byte vs. CSI-u under the
 * kitty report-all-keys flag). */
static uint16_t lt__named_control_key(uint32_t cp) {
  switch (cp) {
  case 0x0D:
    return LT_KEY_ENTER;
  case 0x09:
    return LT_KEY_TAB;
  case 0x08:
    return LT_KEY_BACKSPACE;
  case 0x7F:
    return LT_KEY_BACKSPACE2;
  case 0x1B:
    return LT_KEY_ESC;
  default:
    return 0;
  }
}

/* kitty CSI-u: ESC [ code[:shifted:base] ; mods[:event] [; text] u.
 * The primary code, the mods low bits, the event-type sub-param, and the first
 * text codepoint are used. The text codepoint (sent under the report-
 * associated-text flag) is the layout-translated character — it carries what
 * the keystroke actually typed (Shift+j -> 'J', Caps Lock included), which the
 * primary code alone loses; it wins over the base code for `ch` so the kitty
 * path reports the same character a legacy terminal or the Win32 console
 * would. Returns MATCH/PARTIAL/NOMATCH. Called when seq starts ESC [ <digit>.
 */
static enum lt__key_match lt__match_kitty_u(const unsigned char *seq,
                                            size_t len, struct lt_event *out,
                                            size_t *consumed) {
  int code = 0, mods = 0, event = 1, text = 0;
  bool seen_code = false;
  int field = 0;    /* 0=code, 1=mods, 2=text */
  int subfield = 0; /* within a field, after ':' */
  size_t i = 2;
  for (; i < len; i++) {
    unsigned char c = seq[i];
    if (c >= '0' && c <= '9') {
      if (field == 0 && subfield == 0) {
        code = lt__km_accum(code, c);
        seen_code = true;
      } else if (field == 1 && subfield == 0) {
        mods = lt__km_accum(mods, c);
      } else if (field == 1 && subfield == 1) {
        event = lt__km_accum(event == 1 ? 0 : event, c);
      } else if (field == 2 && subfield == 0) {
        text = lt__km_accum(text, c); /* first text codepoint only */
      }
      /* digits in other sub/fields (shifted-key, extra text codepoints) are
       * ignored */
    } else if (c == ';') {
      field++;
      subfield = 0;
    } else if (c == ':') {
      subfield++;
    } else if (c == 'u') {
      break;
    } else {
      return LT__KEY_NOMATCH;
    }
  }
  if (i == len)
    return LT__KEY_PARTIAL; /* no final u yet */
  if (i + 1 != len || !seen_code)
    return LT__KEY_NOMATCH;

  out->type = LT_EVENT_KEY;
  out->mod = lt__km_mods(mods);
  out->action = (event == 2)   ? LT_KEY_REPEAT
                : (event == 3) ? LT_KEY_RELEASE
                               : LT_KEY_PRESS;

  /* A bare-modifier functional code, or a named control codepoint that kitty
   * reports as 13/9/127/27 under report-all-keys, maps to the same LT_KEY_* the
   * control-byte path produces — so Enter is Enter however it was encoded. */
  uint16_t fkey = lt__km_kitty_functional(code);
  if (!fkey)
    fkey = lt__named_control_key((uint32_t)code);
  if (fkey != 0) {
    out->key = fkey;
    out->ch = 0;
  } else {
    out->key = 0;
    /* Prefer the associated text (layout-translated: Shift+j -> 'J') over the
     * base codepoint; fall back when the terminal sent none (no text flag
     * negotiated, or a release event, which carries no text). */
    out->ch = (text != 0) ? (lt_uchar)text : (lt_uchar)code;
  }
  *consumed = len;
  return LT__KEY_MATCH;
}

/* Standalone control byte (0x00-0x1F or 0x7F), not part of an escape sequence.
 * Compat: termbox2 model (byte in key, ch=0). Modern: keep ENTER/TAB/
 * BACKSPACE named, but normalize Ctrl+letter to a lowercase ch + LT_MOD_CTRL.
 */
static void lt__decode_control_byte(unsigned char b, int input_mode,
                                    struct lt_event *out) {
  out->type = LT_EVENT_KEY;
  out->action = LT_KEY_PRESS;

  if (input_mode & LT_INPUT_COMPAT) {
    out->key = (uint16_t)b;
    out->ch = 0;
    out->mod = 0;
    return;
  }

  /* Modern: distinct named keys win over their Ctrl+letter alias. */
  uint16_t named = lt__named_control_key(b);
  if (named != 0) {
    out->key = named;
    return;
  }
  if (b >= 0x01 && b <= 0x1A) {
    out->ch = (lt_uchar)('a' + (b - 1)); /* Ctrl+A..Z */
    out->mod = LT_MOD_CTRL;
    out->key = 0;
    return;
  }
  if (b >= 0x1C && b <= 0x1F) {
    out->ch = (lt_uchar)(b | 0x40); /* Ctrl+\ ] ^ _ */
    out->mod = LT_MOD_CTRL;
    out->key = 0;
    return;
  }
  /* 0x00 and any leftover: report as key byte. */
  out->key = (uint16_t)b;
  out->ch = 0;
  out->mod = 0;
}

enum lt__key_match lt__key_decode(const unsigned char *seq, size_t len,
                                  int input_mode, struct lt_event *out,
                                  size_t *consumed) {
  if (!seq || !out || !consumed)
    return LT__KEY_NOMATCH;
  if (len == 0)
    return LT__KEY_PARTIAL;

  /* Standalone control byte (never starts an escape sequence; ESC is handled
   * by the sequence paths below). */
  if (len == 1 && seq[0] != 0x1b && (seq[0] < 0x20 || seq[0] == 0x7F)) {
    lt__decode_control_byte(seq[0], input_mode, out);
    *consumed = 1;
    return LT__KEY_MATCH;
  }

  /* Lone ESC is a prefix of every escape sequence. */
  if (len == 1 && seq[0] == 0x1b)
    return LT__KEY_PARTIAL;

  /* OSC (ESC ] ... BEL | ESC \): terminal replies, not key input. The only
   * OSC bytes that reach the decoder are color-query replies
   * (lt_query_color) that landed after the query's deadline — consume them
   * silently instead of shredding them into key events. */
  if (seq[0] == 0x1b && len >= 2 && seq[1] == ']') {
    for (size_t i = 2; i < len; i++) {
      if (seq[i] == 0x07) {
        *consumed = i + 1;
        return LT__KEY_DISCARD;
      }
      if (seq[i] == 0x1b) {
        if (i + 1 >= len)
          return LT__KEY_PARTIAL; /* ESC may begin the ST terminator */
        if (seq[i + 1] == '\\') {
          *consumed = i + 2;
          return LT__KEY_DISCARD;
        }
        return LT__KEY_NOMATCH; /* bare ESC inside payload: malformed */
      }
    }
    return LT__KEY_PARTIAL; /* no terminator yet; read more */
  }

  /* SGR mouse starts ESC [ < ; check before the fixed/param tables. */
  if (seq[0] == 0x1b && len >= 3 && seq[1] == '[' && seq[2] == '<')
    return lt__match_mouse(seq, len, out, consumed);
  if (seq[0] == 0x1b && len == 2 && seq[1] == '[')
    return LT__KEY_PARTIAL; /* ESC [ could still become mouse or a key */

  if (seq[0] == 0x1b) {
    enum lt__key_match m = lt__match_fixed(seq, len, out, consumed);
    if (m != LT__KEY_NOMATCH)
      return m;
  }

  /* Parametric CSI: ESC [ <digits...> <final>. A 'u' final is the kitty/CSI-u
   * key form; ~ / letter finals are the legacy edit/cursor forms. */
  if (seq[0] == 0x1b && len >= 2 && seq[1] == '[') {
    bool has_digit = false;
    for (size_t i = 2; i < len; i++) {
      if (seq[i] >= '0' && seq[i] <= '9') {
        has_digit = true;
        break;
      }
      if (seq[i] == ';' || seq[i] == ':')
        continue;
      break;
    }
    if (has_digit) {
      /* Does the sequence end in 'u'? If a final is present and it is 'u', use
       * the kitty parser; otherwise the legacy param parser. While no final is
       * present yet, both report PARTIAL, so either is fine. */
      unsigned char last = seq[len - 1];
      enum lt__key_match m;
      if (last == 'u')
        m = lt__match_kitty_u(seq, len, out, consumed);
      else
        m = lt__match_csi_param(seq, len, out, consumed);
      if (m != LT__KEY_NOMATCH)
        return m;
      /* A CSI is terminated by a final byte (0x40-0x7E). If the last byte isn't
       * a final yet (a parameter/intermediate byte, < 0x40), more is coming —
       * wait. Otherwise it is a complete but unrecognized sequence; fall
       * through to NOMATCH so the caller stops waiting immediately. */
      if (last < 0x40)
        return LT__KEY_PARTIAL;
    }
  }

  return LT__KEY_NOMATCH;
}
