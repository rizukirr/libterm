#include "internal.h"
#include "libterm/libterm.h"
#include "platform.h"
#include "posix_internal.h"
#include "posix_resize.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

/* Inter-byte grace period while assembling a multi-byte input sequence (escape
 * sequence or UTF-8 character). A terminal can deliver the bytes fragmented
 * (SSH, a loaded host, a slow pipe); a zero timeout would see an empty fd
 * between fragments and truncate the sequence — misreporting an arrow key as a
 * bare ESC, or a multi-byte character as U+FFFD. This is also the standard
 * "lone ESC vs. start of a sequence" disambiguation window: a real Escape
 * keypress costs at most this much latency before it resolves. */
#define LT__ESC_SEQ_TIMEOUT_MS 50

/* Saturating decimal accumulation for escape-sequence numeric fields. Inputs
 * arrive from the terminal (untrusted), so a long run of digits must not
 * overflow the int accumulator — signed overflow is undefined behavior. We cap
 * well below INT_MAX; any value past this is already meaningless for a key
 * code, coordinate, or modifier field, so saturating is correct, not just safe.
 * (The SGR mouse parser open-codes the same guard.) */
#define LT__CSI_NUM_CAP 1000000
static int lt__accum_digit(int acc, unsigned char digit) {
  if (acc < LT__CSI_NUM_CAP)
    return acc * 10 + (digit - '0');
  return acc;
}

/* Bytes pending re-delivery as standalone events, replayed one at a time on the
 * following reads. Two producers stash here: LT_INPUT_ESC (an unrecognized
 * ESC-prefixed combo emits LT_KEY_ESC, then the trailing byte) and the UTF-8
 * assembler (a stray non-continuation byte, instead of swallowing it). The
 * consumer, lt__posix_pending_pop, sits lower down next to where it is drained.
 */
static unsigned char lt__posix_pending[8];
static size_t lt__posix_pending_len = 0;
static size_t lt__posix_pending_pos = 0;

static void lt__posix_pending_push(const unsigned char *bytes, size_t n) {
  if (n > sizeof(lt__posix_pending))
    n = sizeof(lt__posix_pending);
  memcpy(lt__posix_pending, bytes, n);
  lt__posix_pending_len = n;
  lt__posix_pending_pos = 0;
}

/* Raw bytes captured off the tty while a color query (lt__plat_query_color)
 * was waiting for its reply, re-consumed as the byte source AHEAD of the fd
 * by the readers below — so an escape sequence typed during the query window
 * re-assembles into its real event. Distinct from lt__posix_pending above,
 * which replays bytes as standalone EVENTS (the LT_INPUT_ESC two-event
 * model); routing those through the decoder would re-fold Alt combos. */
#define LT__PUSHBACK_CAP 64
static unsigned char lt__posix_pushback[LT__PUSHBACK_CAP];
static size_t lt__posix_pushback_head = 0; /* read position */
static size_t lt__posix_pushback_tail = 0; /* write position */

static void lt__posix_pushback_push(unsigned char b) {
  size_t next = (lt__posix_pushback_tail + 1) % LT__PUSHBACK_CAP;
  if (next == lt__posix_pushback_head)
    return; /* full: drop the newest byte (bounded loss, same policy as
               lt__posix_pending's truncating push) */
  lt__posix_pushback[lt__posix_pushback_tail] = b;
  lt__posix_pushback_tail = next;
}

static bool lt__posix_pushback_pop(unsigned char *out) {
  if (lt__posix_pushback_head == lt__posix_pushback_tail)
    return false;
  *out = lt__posix_pushback[lt__posix_pushback_head];
  lt__posix_pushback_head = (lt__posix_pushback_head + 1) % LT__PUSHBACK_CAP;
  return true;
}

static size_t lt__posix_read_utf8_tail(unsigned char *buf, size_t have,
                                       size_t want) {
  size_t len = have;
  int ttyfd = lt__posix_get_tty_fd();
  if (ttyfd < 0)
    return len;

  fd_set rfds = {0};
  struct timeval tv = {0};
  int rc = 0;

  while (len < want) {
    unsigned char pb;
    if (lt__posix_pushback_pop(&pb)) {
      /* Same continuation check as the fd path below. */
      if ((pb & 0xC0) != 0x80) {
        lt__posix_pending_push(&pb, 1);
        break;
      }
      buf[len++] = pb;
      continue;
    }
    /* Same grace period as the escape-sequence reader: a multi-byte UTF-8
     * character can arrive fragmented, and a zero timeout would truncate it
     * into a spurious U+FFFD. See LT__ESC_SEQ_TIMEOUT_MS. */
    tv.tv_sec = 0;
    tv.tv_usec = LT__ESC_SEQ_TIMEOUT_MS * 1000;

    FD_ZERO(&rfds);
    FD_SET(ttyfd, &rfds);
    rc = select(ttyfd + 1, &rfds, NULL, NULL, &tv);
    if (rc < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    if (rc == 0)
      break;

    ssize_t n = read(ttyfd, &buf[len], 1);
    if (n != 1)
      break;

    /* A byte that isn't a UTF-8 continuation (10xxxxxx) does not belong to this
     * sequence. Stash it for re-delivery as its own event and stop, so the
     * partial sequence decodes to U+FFFD without swallowing the next keypress.
     */
    if ((buf[len] & 0xC0) != 0x80) {
      unsigned char stray = buf[len];
      lt__posix_pending_push(&stray, 1);
      break;
    }

    len++;
  }

  return len;
}

/* Read one more byte within the inter-byte grace period. Returns true and sets
 * *out on success; false on timeout/EOF/error. Mirrors the timeout used by the
 * UTF-8 and escape-tail readers. */
static bool lt__posix_read_one_grace(unsigned char *out) {
  if (lt__posix_pushback_pop(out))
    return true;
  int ttyfd = lt__posix_get_tty_fd();
  if (ttyfd < 0)
    return false;
  for (;;) {
    struct timeval tv = {.tv_sec = 0, .tv_usec = LT__ESC_SEQ_TIMEOUT_MS * 1000};
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(ttyfd, &rfds);
    int rc = select(ttyfd + 1, &rfds, NULL, NULL, &tv);
    if (rc < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    if (rc == 0)
      return false;
    ssize_t k = read(ttyfd, out, 1);
    return k == 1;
  }
}

static uint8_t lt__posix_parse_csi_mod(int mod_code) {
  switch (mod_code) {
  case 2:
    return LT_MOD_SHIFT;
  case 3:
    return LT_MOD_ALT;
  case 4:
    return LT_MOD_SHIFT | LT_MOD_ALT;
  case 5:
    return LT_MOD_CTRL;
  case 6:
    return LT_MOD_SHIFT | LT_MOD_CTRL;
  case 7:
    return LT_MOD_ALT | LT_MOD_CTRL;
  case 8:
    return LT_MOD_SHIFT | LT_MOD_ALT | LT_MOD_CTRL;
  default:
    return 0;
  }
}

static void lt__posix_parse_csi_final_key(unsigned char final,
                                          struct lt_event *ev) {
  switch (final) {
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
  case 'H':
    ev->key = LT_KEY_HOME;
    break;
  case 'F':
    ev->key = LT_KEY_END;
    break;
  case 'Z':
    /* CSI Z = back-tab (Shift+Tab). Confirmed by tmux input-keys.c:121-122
     * (KEYC_BTAB -> "\033[Z") and the kitty keyboard protocol. */
    ev->key = LT_KEY_BACK_TAB;
    break;
  default:
    break;
  }
}

static void lt__posix_parse_ss3_final_key(unsigned char final,
                                          struct lt_event *ev) {
  switch (final) {
  case 'H':
    ev->key = LT_KEY_HOME;
    break;
  case 'F':
    ev->key = LT_KEY_END;
    break;
  case 'P':
    ev->key = LT_KEY_F1;
    break;
  case 'Q':
    ev->key = LT_KEY_F2;
    break;
  case 'R':
    ev->key = LT_KEY_F3;
    break;
  case 'S':
    ev->key = LT_KEY_F4;
    break;
  default:
    break;
  }
}

static int lt__posix_parse_csi_letter_mod(const unsigned char *seq,
                                          size_t seq_len) {
  int mod_code = 0;
  bool seen_semi = false;

  for (size_t i = 2; i + 1 < seq_len; i++) {
    if (seq[i] == ';') {
      seen_semi = true;
      mod_code = 0;
      continue;
    }

    if (seq[i] < '0' || seq[i] > '9')
      return 0;

    if (seen_semi)
      mod_code = lt__accum_digit(mod_code, seq[i]);
  }

  return seen_semi ? mod_code : 0;
}

static void lt__posix_parse_csi_tilde_key(const unsigned char *seq,
                                          size_t seq_len, struct lt_event *ev) {
  int code = 0, mod_code = 0;
  bool in_modifier = false;

  for (size_t i = 2; i + 1 < seq_len; i++) {
    if (seq[i] == ';') {
      in_modifier = true;
      continue;
    }

    if (seq[i] < '0' || seq[i] > '9') {
      code = 0;
      mod_code = 0;
      break;
    }

    if (!in_modifier)
      code = lt__accum_digit(code, seq[i]);
    else
      mod_code = lt__accum_digit(mod_code, seq[i]);
  }

  ev->mod = lt__posix_parse_csi_mod(mod_code);

  switch (code) {
  case 2:
    ev->key = LT_KEY_INSERT;
    break;
  case 3:
    ev->key = LT_KEY_DELETE;
    break;
  case 5:
    ev->key = LT_KEY_PGUP;
    break;
  case 6:
    ev->key = LT_KEY_PGDN;
    break;
  case 15:
    ev->key = LT_KEY_F5;
    break;
  case 17:
    ev->key = LT_KEY_F6;
    break;
  case 18:
    ev->key = LT_KEY_F7;
    break;
  case 19:
    ev->key = LT_KEY_F8;
    break;
  case 20:
    ev->key = LT_KEY_F9;
    break;
  case 21:
    ev->key = LT_KEY_F10;
    break;
  case 23:
    ev->key = LT_KEY_F11;
    break;
  case 24:
    ev->key = LT_KEY_F12;
    break;
  default:
    break;
  }
}

/* termbox2 model: a standalone control byte (0x00-0x1F or 0x7F) is reported as
 * a key code with ch == 0 (LT_KEY_CTRL_*, which share values with the named
 * ENTER/TAB/BACKSPACE keys), not as a character. ESC (0x1B) is intercepted by
 * the escape-sequence path before this is reached. Returns true if `b` was a
 * control byte and the event was filled. */
static bool lt__posix_ctrl_byte_event(unsigned char b, struct lt_event *ev) {
  if (b >= 0x20 && b != 0x7F)
    return false;

  ev->type = LT_EVENT_KEY;
  ev->mod = 0;
  ev->key = (uint16_t)b;
  ev->ch = 0;
  return true;
}

/* Fill `ev` from the next pending byte. Returns true while bytes remain. Alt
 * combos are ASCII in practice, so a pending byte is either a control byte
 * (-> key code) or a single character (-> ch). */
static bool lt__posix_pending_pop(struct lt_event *ev) {
  if (lt__posix_pending_pos >= lt__posix_pending_len)
    return false;

  unsigned char b = lt__posix_pending[lt__posix_pending_pos++];
  if (lt__posix_pending_pos >= lt__posix_pending_len) {
    lt__posix_pending_len = 0;
    lt__posix_pending_pos = 0;
  }

  ev->type = LT_EVENT_KEY;
  if (!lt__posix_ctrl_byte_event(b, ev))
    ev->ch = (lt_uchar)b;
  return true;
}

/* Handle an ESC-prefixed sequence that no recognized-key branch claimed — i.e.
 * Alt+<key>, which a legacy terminal encodes as ESC then the key byte(s).
 * (On a kitty-capable terminal Alt arrives as a CSI-u modifier and never
 * reaches here.) Behavior:
 *   fold  -> one event, the key with LT_MOD_ALT set. Used in the modern model
 *            (default) and in compat mode with LT_INPUT_ALT.
 *   split -> emit LT_KEY_ESC now; replay the trailing byte(s) as the next
 *            event(s). termbox2's two-event default, used in compat mode
 *            without LT_INPUT_ALT (LT_INPUT_ESC). */
static int lt__posix_handle_alt_combo(const unsigned char *seq, size_t seq_len,
                                      struct lt_event *ev) {
  bool fold = !(lt__g.input_mode & LT_INPUT_COMPAT) ||
              (lt__g.input_mode & LT_INPUT_ALT);
  if (fold) {
    unsigned char b = seq[1];
    if (!lt__posix_ctrl_byte_event(b, ev))
      ev->ch = (lt_uchar)b;
    ev->mod |= LT_MOD_ALT;
    return LT_OK;
  }

  /* compat mode, LT_INPUT_ESC (the termbox2 two-event default). */
  ev->type = LT_EVENT_KEY;
  ev->mod = 0;
  ev->key = LT_KEY_ESC;
  ev->ch = 0;
  if (seq_len > 1)
    lt__posix_pending_push(seq + 1, seq_len - 1);
  return LT_OK;
}

/* Parse an SGR (1006) mouse report: ESC [ < Cb ; Cx ; Cy (M|m), where Cb is the
 * button code and Cx/Cy are 1-based column/row. Fills ev (type LT_EVENT_MOUSE,
 * key = LT_KEY_MOUSE_*, x/y 0-based, mod from the Cb modifier bits) and returns
 * true on a well-formed report; returns false (leaving ev untouched) otherwise,
 * so the caller can fall through to the key paths. Mirrors termbox2's TYPE_1006
 * branch (termbox2.h:3690-3755). */
static bool lt__posix_parse_mouse_sgr(const unsigned char *seq, size_t seq_len,
                                      struct lt_event *ev) {
  /* Shortest valid report is ESC [ < 0 ; 0 ; 0 M = 9 bytes. */
  if (seq_len < 9 || seq[1] != '[' || seq[2] != '<')
    return false;

  unsigned char final = seq[seq_len - 1];
  if (final != 'M' && final != 'm')
    return false;

  int num[3] = {0, 0, 0};
  int num_i = 0;
  bool seen_digit = false;

  for (size_t i = 3; i < seq_len - 1; i++) {
    unsigned char c = seq[i];
    if (c >= '0' && c <= '9') {
      /* Saturating accumulation (see lt__accum_digit): untrusted digit runs
       * must not overflow the int. */
      num[num_i] = lt__accum_digit(num[num_i], c);
      seen_digit = true;
    } else if (c == ';') {
      if (!seen_digit || num_i >= 2)
        return false;
      num_i++;
      seen_digit = false;
    } else {
      return false;
    }
  }

  /* Need exactly three fields, the last one non-empty. */
  if (num_i != 2 || !seen_digit)
    return false;

  int cb = num[0];

  /* Low two bits select the button; bit 6 (64) promotes 0/1 to wheel up/down.
   */
  switch (cb & 3) {
  case 0:
    ev->key = (cb & 64) ? LT_KEY_MOUSE_WHEEL_UP : LT_KEY_MOUSE_LEFT;
    break;
  case 1:
    ev->key = (cb & 64) ? LT_KEY_MOUSE_WHEEL_DOWN : LT_KEY_MOUSE_MIDDLE;
    break;
  case 2:
    ev->key = LT_KEY_MOUSE_RIGHT;
    break;
  default: /* 3 */
    ev->key = LT_KEY_MOUSE_RELEASE;
    break;
  }

  /* A lowercase 'm' final is xterm's button-release marker. */
  if (final == 'm')
    ev->key = LT_KEY_MOUSE_RELEASE;

  /* Modifier bits: shift=4, alt=8, ctrl=16; motion=32 (drag). */
  ev->mod = 0;
  if (cb & 4)
    ev->mod |= LT_MOD_SHIFT;
  if (cb & 8)
    ev->mod |= LT_MOD_ALT;
  if (cb & 16)
    ev->mod |= LT_MOD_CTRL;
  if (cb & 32)
    ev->mod |= LT_MOD_MOTION;

  /* Reports are 1-based; clamp to 0 so a stray 0 coordinate can't go negative.
   */
  ev->x = num[1] > 0 ? num[1] - 1 : 0;
  ev->y = num[2] > 0 ? num[2] - 1 : 0;
  ev->type = LT_EVENT_MOUSE;
  return true;
}

/* Parse a CSI-u key report: ESC [ number ; modifiers u, the modern
 * (fixterms / kitty / recent xterm) unambiguous key encoding. `number` is the
 * key's Unicode codepoint; `modifiers` is 1 + a bitmask (shift=1, alt=2,
 * ctrl=4, plus higher bits we have no LT_MOD_* for). On a well-formed report
 * this fills ev (ch = codepoint, mod from the low bits) and returns true;
 * otherwise returns false so the caller can try the other key paths.
 * Reference: external/kitty/docs/keyboard-protocol.rst (CSI format + modifier
 * table, "1 + actual modifiers"). */
static bool lt__posix_parse_csi_u(const unsigned char *seq, size_t seq_len,
                                  struct lt_event *ev) {
  /* Shortest is ESC [ <digit> u = 4 bytes. */
  if (seq_len < 4 || seq[1] != '[' || seq[seq_len - 1] != 'u')
    return false;

  int code = 0, mod_field = 0;
  bool seen_code = false, in_mod = false;

  for (size_t i = 2; i < seq_len - 1; i++) {
    unsigned char c = seq[i];
    if (c >= '0' && c <= '9') {
      if (in_mod)
        mod_field = lt__accum_digit(mod_field, c);
      else {
        code = lt__accum_digit(code, c);
        seen_code = true;
      }
    } else if (c == ';' && !in_mod) {
      in_mod = true;
    } else {
      return false; /* anything else (incl. a second ';' sub-field) -> not CSI-u
                     */
    }
  }

  if (!seen_code)
    return false;

  /* modifiers field is 1 + bitmask; absent means 1 (no modifiers). Decode the
   * low three bits we model; ignore super/hyper/meta/locks. */
  ev->mod = 0;
  if (mod_field > 0) {
    int bits = mod_field - 1;
    if (bits & 1)
      ev->mod |= LT_MOD_SHIFT;
    if (bits & 2)
      ev->mod |= LT_MOD_ALT;
    if (bits & 4)
      ev->mod |= LT_MOD_CTRL;
  }

  ev->type = LT_EVENT_KEY;
  ev->ch = (lt_uchar)code;
  return true;
}

static unsigned int lt__posix_parse_esc_seq(const unsigned char *seq,
                                            size_t seq_len,
                                            struct lt_event *ev) {

  if (lt__posix_parse_mouse_sgr(seq, seq_len, ev))
    return LT_OK;

  if (lt__posix_parse_csi_u(seq, seq_len, ev))
    return LT_OK;

  if (seq_len == 3 && seq[1] == '[') {
    lt__posix_parse_csi_final_key(seq[2], ev);
  } else if (seq_len == 3 && seq[1] == 'O') {
    lt__posix_parse_ss3_final_key(seq[2], ev);
  } else if (seq_len >= 5 && seq[1] == '[' &&
             (seq[seq_len - 1] == 'A' || seq[seq_len - 1] == 'B' ||
              seq[seq_len - 1] == 'C' || seq[seq_len - 1] == 'D' ||
              seq[seq_len - 1] == 'H' || seq[seq_len - 1] == 'F')) {
    int mod_code = lt__posix_parse_csi_letter_mod(seq, seq_len);
    ev->mod = lt__posix_parse_csi_mod(mod_code);
    lt__posix_parse_csi_final_key(seq[seq_len - 1], ev);
  } else if (seq_len >= 4 && seq[1] == '[') {
    if (seq[seq_len - 1] == '~') {
      lt__posix_parse_csi_tilde_key(seq, seq_len, ev);
    }
  }

  if (seq_len == 1 || (seq_len == 2 && (seq[1] == '\n' || seq[1] == '\r')))
    ev->key = LT_KEY_ESC;

  ev->ch = (ev->key == 0 && seq_len == 1) ? (lt_uchar)seq[0] : 0;
  return LT_OK;
}

int lt__plat_read_event(struct lt_event *ev, int timeout_ms) {
  if (!ev)
    return LT_ERR;

  int ttyfd = lt__posix_get_tty_fd();
  if (ttyfd < 0)
    return LT_ERR_POLL;

  int rc = 0, new_w = 0, new_h = 0, szrc = 0, rrc = 0;
  int rfd = lt__posix_resize_read_fd();
  lt_uchar ch = 0;
  ssize_t n = 0;

  struct timeval tv;
  struct timeval *tv_ptr = NULL;

  while (true) {
    memset(ev, 0, sizeof(*ev));

    /* Replay any bytes stashed by an LT_INPUT_ESC Alt-combo before blocking. */
    if (lt__posix_pending_pop(ev))
      return LT_OK;

    unsigned char pb;
    if (lt__posix_pushback_pop(&pb)) {
      /* Replay a byte stashed during a color query as if freshly read. */
      ch = (lt_uchar)pb;
      n = 1;
    } else {
      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(ttyfd, &rfds);
      if (rfd >= 0)
        FD_SET(rfd, &rfds);

      memset(&tv, 0, sizeof(tv));
      tv_ptr = NULL;

      if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tv_ptr = &tv;
      }

      int maxfd = ttyfd;
      if (rfd > maxfd)
        maxfd = rfd;

      rc = select(maxfd + 1, &rfds, NULL, NULL, tv_ptr);
      if (rc == 0)
        return LT_ERR_NO_EVENT;

      if (rc < 0) {
        if (errno == EINTR)
          return LT_ERR_NO_EVENT;
        lt__g.last_errno = errno;
        return LT_ERR_POLL;
      }

      if (rfd >= 0 && FD_ISSET(rfd, &rfds)) {
        unsigned char drain[64];
        while (read(rfd, drain, sizeof(drain)) > 0) {
        }

        szrc = lt__plat_get_size(&new_w, &new_h);
        if (szrc != LT_OK)
          continue;

        rrc = lt__handle_resize(new_w, new_h, ev);
        if (rrc == LT_ERR_NO_EVENT)
          continue; /* spurious/unchanged: keep waiting */
        return rrc; /* LT_OK with the event filled, or the buffer error */
      }

      n = read(ttyfd, &ch, 1);
    }

    if (n > 0) {
      /* ESC or a standalone control byte -> the shared decoder. Printable and
       * UTF-8 lead bytes fall through to the UTF-8 assembler below. */
      if (ch == '\x1b' || (unsigned char)ch < 0x20 ||
          (unsigned char)ch == 0x7F) {
        unsigned char seq[64];
        size_t len = 0;
        seq[len++] = (unsigned char)ch;

        for (;;) {
          struct lt_event tmp;
          memset(&tmp, 0, sizeof(tmp));
          size_t consumed = 0;
          enum lt__key_match m =
              lt__key_decode(seq, len, lt__g.input_mode, &tmp, &consumed);

          if (m == LT__KEY_MATCH) {
            *ev = tmp;
            /* Stash any bytes past the matched sequence for replay. */
            if (consumed < len)
              lt__posix_pending_push(seq + consumed, len - consumed);
            return LT_OK;
          }

          if (m == LT__KEY_DISCARD) {
            /* Recognized bytes (a stray OSC reply) that yield no event. */
            if (consumed < len) {
              memmove(seq, seq + consumed, len - consumed);
              len -= consumed;
              continue; /* re-decode the remainder */
            }
            goto outer_continue; /* nothing left: wait for fresh input */
          }

          if (m == LT__KEY_PARTIAL && len < sizeof(seq)) {
            unsigned char nb;
            if (lt__posix_read_one_grace(&nb)) {
              seq[len++] = nb;
              continue;
            }
            /* Grace expired: finalize what we have. */
          }

          /* NOMATCH, buffer full, or partial that never completed: fall back to
           * the legacy ESC/Alt-combo handling for ESC-prefixed input. */
          if (seq[0] == 0x1b) {
            ev->type = LT_EVENT_KEY;
            ev->mod = 0;
            ev->key = 0;
            lt__posix_parse_esc_seq(seq, len, ev);
            if (ev->key == 0 && ev->ch == 0 && len >= 2 && seq[1] != '[' &&
                seq[1] != 'O')
              return lt__posix_handle_alt_combo(seq, len, ev);
            return LT_OK;
          }

          /* A lone control byte that didn't match (shouldn't happen): report
           * it raw. */
          ev->type = LT_EVENT_KEY;
          ev->key = (uint16_t)seq[0];
          ev->ch = 0;
          ev->mod = 0;
          return LT_OK;
        }
      }

      unsigned char seq8[4] = {(unsigned char)ch, 0, 0, 0};
      int need = lt__utf8_char_length((char)seq8[0]);

      if (need <= 0 || need > 4) {
        ev->type = LT_EVENT_KEY;
        ev->mod = 0;
        ev->key = 0;
        ev->ch = 0xFFFD;
        return LT_OK;
      }

      size_t have = 1;
      if (need > 1)
        have = lt__posix_read_utf8_tail(seq8, have, (size_t)need);

      lt_uchar cp = 0;
      int dec = lt__utf8_decode((const char *)seq8, have, &cp);

      ev->type = LT_EVENT_KEY;
      ev->mod = 0;
      ev->key = 0;

      if (dec == need) {
        ev->ch = cp;
      } else {
        ev->ch = 0xFFFD;
      }

      return LT_OK;
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

    lt__g.last_errno = errno;
    return LT_ERR_READ;
  outer_continue:;
  }
  return LT_OK;
}

int lt__plat_get_fds(int *ttyfd, int *resizefd) {
  if (ttyfd)
    *ttyfd = lt__posix_get_tty_fd();
  if (resizefd)
    *resizefd = lt__posix_resize_read_fd();
  return LT_OK;
}

int lt__plat_set_mouse(int enable) {
  if (enable) {
    static const char on[] = "\x1b[?1000h\x1b[?1006h";
    return lt__plat_write(on, sizeof(on) - 1);
  }
  static const char off[] = "\x1b[?1006l\x1b[?1000l";
  return lt__plat_write(off, sizeof(off) - 1);
}

int lt__plat_query_color(int what, uint32_t *rgb, int timeout_ms) {
  int ttyfd = lt__posix_get_tty_fd();
  if (ttyfd < 0)
    return LT_ERR_POLL;

  /* Emit the query: OSC 10/11 for the defaults, OSC 4;<idx> for the palette.
   * BEL-terminated; replies may use BEL or ST and we accept both. */
  char q[24];
  int qlen;
  if (what == LT_COLOR_DEFAULT_FG) {
    memcpy(q, "\x1b]10;?\x07", 7);
    qlen = 7;
  } else if (what == LT_COLOR_DEFAULT_BG) {
    memcpy(q, "\x1b]11;?\x07", 7);
    qlen = 7;
  } else {
    qlen = snprintf(q, sizeof(q), "\x1b]4;%d;?\x07", what);
  }

  int rc = lt__plat_write(q, (size_t)qlen);
  if (rc != LT_OK)
    return rc;
  rc = lt__plat_flush();
  if (rc != LT_OK)
    return rc;

  long long deadline = lt__posix_now_ms() + timeout_ms;

  /* Scan the input stream for the reply. Bytes that aren't the reply (the
   * user typing during the window) go to the pushback ring, replayed by the
   * normal event loop afterwards. A reply to a different OSC number, or a
   * malformed one, is discarded and the wait continues. */
  enum { SCAN, ESC_SEEN, COLLECT, COLLECT_ESC } st = SCAN;
  char reply[64];
  size_t rlen = 0;
  bool overflow = false;

  for (;;) {
    long long remaining = deadline - lt__posix_now_ms();
    if (remaining <= 0)
      return LT_ERR_NO_EVENT;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(ttyfd, &rfds);
    struct timeval tv = {.tv_sec = (time_t)(remaining / 1000),
                         .tv_usec = (suseconds_t)((remaining % 1000) * 1000)};
    int src = select(ttyfd + 1, &rfds, NULL, NULL, &tv);
    if (src < 0) {
      if (errno == EINTR)
        continue;
      lt__g.last_errno = errno;
      return LT_ERR_POLL;
    }
    if (src == 0)
      return LT_ERR_NO_EVENT;

    unsigned char b;
    ssize_t n = read(ttyfd, &b, 1);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      lt__g.last_errno = errno;
      return LT_ERR_READ;
    }
    if (n == 0)
      return LT_ERR_NO_EVENT; /* EOF: no reply is coming */

    bool reply_done = false;
    switch (st) {
    case SCAN:
      if (b == 0x1b)
        st = ESC_SEEN;
      else
        lt__posix_pushback_push(b);
      break;
    case ESC_SEEN:
      if (b == ']') {
        st = COLLECT;
        rlen = 0;
        overflow = false;
      } else {
        /* Not a reply: both bytes belong to the user's input. */
        lt__posix_pushback_push(0x1b);
        lt__posix_pushback_push(b);
        st = SCAN;
      }
      break;
    case COLLECT:
      if (b == 0x07) { /* BEL-terminated reply */
        reply_done = true;
        st = SCAN;
      } else if (b == 0x1b) {
        st = COLLECT_ESC;
      } else if (rlen < sizeof(reply)) {
        reply[rlen++] = (char)b;
      } else {
        overflow = true; /* overlong: drain to the terminator, then drop */
      }
      break;
    case COLLECT_ESC:
      if (b == '\\') { /* ST-terminated reply */
        reply_done = true;
        st = SCAN;
      } else {
        st = SCAN; /* malformed ESC inside the payload */
      }
      break;
    }

    if (reply_done) {
      if (!overflow &&
          lt__color_parse_osc_reply(reply, rlen, what, rgb) == LT_OK)
        return LT_OK;
      /* wrong number / malformed / overlong: keep waiting for the deadline */
    }
  }
}
