# libterm — Roadmap

Tracks the full termbox2 public API, renamed from the `tb_` / `TB_` prefix to `lt_` / `LT_`, and the current implementation status of each symbol on POSIX and Windows.

A symbol counts as **working** only when it actually performs the documented effect end-to-end on a real terminal — declarations and no-op stubs do not count. POSIX and Windows are tracked independently.

Legend: `[x]` working · `[~]` partial / stubbed · `[ ]` not implemented · `[—]` not applicable

---

## Public functions

### Lifecycle

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_init` | `lt_init` | [x] | [x] | POSIX now opens `/dev/tty`, enters raw mode, enters alt-screen, and initializes SIGWINCH self-pipe. Windows enters raw mode + alt-screen (`\x1b[?1049h`) so prior scrollback is preserved across the libterm session |
| `tb_init_file` | `lt_init_file` | [x] | [—] | Opens `path` (`O_RDWR`) and inits against it; libterm owns and closes the fd on shutdown (unlike `lt_init_fd`). `LT_ERR_INIT_OPEN` if it can't open or isn't a tty (`lt_last_errno` carries the cause). Shares the POSIX open+own path with `lt__plat_init` (`/dev/tty`). Tested against a real pty device path in `tests/test_init_file.c`. Windows: returns `LT_ERR_INIT_OPEN` (path/fd init is POSIX-only) |
| `tb_init_fd` | `lt_init_fd` | [x] | [—] | Inits against a caller-supplied tty fd (`isatty`-validated; **not** closed on shutdown — caller retains ownership). Mirrors termbox2's `tb_init_fd`; `lt_init` now delegates to it via shared `lt__finish_init`. Windows: stub returns `LT_ERR_INIT_OPEN` (POSIX-only). Exercised headlessly by `tests/test_pty_lifecycle.c` |
| `tb_init_rwfd` | `lt_init_rwfd` | [—] | [—] | **Intentional divergence.** libterm's POSIX layer is single-fd by design (input reads and output writes share one tty fd). Split read/write fds would thread a second fd through init/output/input for a use case (terminal over two different fds) that doesn't arise in practice. Use `lt_init_fd` / `lt_init_file` |
| `tb_shutdown` | `lt_shutdown` | [x] | [x] | POSIX restores cursor visibility, leaves alt-screen, restores saved termios, tears down resize pipe/signal state, and closes tty fd. Windows leaves alt-screen (`\x1b[?1049l`) after cursor-show + flush, then restores both console modes |

### Screen geometry & rendering

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_width` | `lt_width` | [x] | [x] | Reads cached `lt__g.width` |
| `tb_height` | `lt_height` | [x] | [x] | Reads cached `lt__g.height` |
| `tb_clear` | `lt_clear` | [x] | [x] | Clears back buffer with current clear attrs |
| `tb_set_clear_attrs` | `lt_set_clear_attrs` | [x] | [x] | Returns `LT_ERR_NOT_INIT` before `lt_init` (consistent with `lt_clear`/`lt_present`/`lt_set_cell`) |
| `tb_present` | `lt_present` | [x] | [x] | Shared diff loop skips cells where `back == front` (3-field equality), caches cursor position, coalesces runs, and flushes buffered output. The run/SGR emission now lives in **shared** `src/shared/sgr.c` (`lt__render_run` → `lt__emit_sgr`), so both platforms emit the same mode-aware SGR for all five output modes — normal/256/216/grayscale/truecolor — with a `LT_HI_BLACK` sentinel separating terminal-default from real black. Output bytes asserted on POSIX via pty (`tests/test_posix_sgr_output.c`); Windows uses the identical shared code (real-terminal visual confirmation pending, no Windows-native byte test yet) |
| `tb_invalidate` | `lt_invalidate` | [x] | [x] | Shared (`src/shared/output.c`): sets a `force_repaint` flag so the next `lt_present` repaints every cell (bypassing the diff) and re-emits SGR from scratch; consumed once. For recovering after raw `lt_send` (or other out-of-band writes) desync libterm's screen model. Tested in `tests/test_invalidate.c` |
| `tb_set_cursor` | `lt_set_cursor` | [x] | [x] | Both platforms emit `\x1b[r;cH` with hand-rolled integer formatting (no `snprintf` in render hot path). termbox2 visibility parity: the cursor starts **hidden** at init; `lt_set_cursor` records the user cursor, implies show, validates (out-of-range → `LT_ERR_OUT_OF_BOUNDS`, the shipped contract), and `lt_present` reparks the physical cursor there after painting; the repark clamps if a resize shrank the viewport (byte-asserted in `tests/test_cursor_visibility.c`) |
| `tb_hide_cursor` | `lt_hide_cursor` | [x] | [x] | Hides and clears the user cursor; idempotent (already-hidden emits nothing). The init default |
| *(libterm addition)* | `lt_show_cursor` | [x] | [x] | Shows at the current physical position and adopts it as the user cursor (state coherence). No termbox2 equivalent — prefer `lt_set_cursor` for ported code |
| `tb_set_cell` | `lt_set_cell` | [x] | [x] | Bounds-checked write into back buffer |
| `tb_set_cell_ex` | `lt_set_cell_ex` | [x] | [x] | Writes a grapheme cluster (base + trailing combining/joined codepoints) into one cell. Shared (`src/shared/cell.c` + cluster table `src/shared/egc.c`): the base stays in `ch`, the trailing codepoints are interned to a content-deduped id kept in `_reserved` (so the 16-byte SIMD diff invariant holds — equal id ⇔ equal cluster). Renderer emits base + continuation codepoints. Tested in `tests/test_egc.c` |
| `tb_extend_cell` | `lt_extend_cell` | [x] | [x] | Appends one codepoint to the cluster in a cell (rebuilds + re-interns via `lt_set_cell_ex`). Cluster length capped at 16 codepoints. Tested in `tests/test_egc.c` |
| `tb_get_cell` | `lt_get_cell` | [x] | [x] | Shared (`src/shared/cell.c`): copies the back-buffer cell at (x, y) into a caller `struct lt_cell` (value copy, not an internal pointer like termbox2). Bounds-checked; `LT_ERR_NOT_INIT` before init. Tested in `tests/test_get_cell.c` |

### Input

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_poll_event` | `lt_poll_event` | [x] | [x] | POSIX decodes input through the shared pure decoder (`src/shared/keymap.c`, `lt__key_decode`): a fixed-sequence table (xterm normal cursor, SS3 incl. F1–F4, rxvt Shift/Ctrl arrows, Linux-console double-bracket F1–F5, back-tab) plus a parametric CSI parser (tilde edit/function keys + `ESC[1;mods` modifier suffixes), the full kitty CSI-u form (codepoint + modifiers + press/repeat/release event types + bare-modifier functional codes), SGR (1006) mouse reports (button/wheel/coords/modifiers → `LT_EVENT_MOUSE`), standalone control bytes, SIGWINCH resize events, and UTF-8 assembly with `U+FFFD` fallback. Broadened terminal coverage (xterm/SS3/rxvt/linux/tilde + modifier suffixes). Kitty progressive enhancement is negotiated by default (`lt__plat_kitty_enable`), with silent fallback to legacy parsing on non-supporting terminals; `LT_INPUT_COMPAT` opts back into termbox2 control-byte semantics |
| `tb_peek_event` | `lt_peek_event` | [x] | [x] | Same shared-decoder path and coverage as `lt_poll_event` (non-blocking variant) |
| `tb_get_fds` | `lt_get_fds` | [x] | [—] | Fills `*ttyfd` / `*resizefd` (either may be NULL) with the terminal fd and SIGWINCH self-pipe read end, so a consumer can wait on libterm input in its own select/poll/epoll loop and drain with `lt_peek_event(ev, 0)`. POSIX via `lt__plat_get_fds`; Windows returns `LT_ERR_UNSUPPORTED_TERM` (console handles aren't pollable fds). Tested in `tests/test_get_fds.c` (real `select()` round-trip) |
| `tb_set_input_mode` | `lt_set_input_mode` | [x] | [x] | `lt_init` defaults to `LT_INPUT_ESC`. POSIX honors the flag for Alt-combos (`ESC <byte>`): `LT_INPUT_ALT` → one event with `LT_MOD_ALT`; `LT_INPUT_ESC` → `LT_KEY_ESC` then the byte replayed (termbox2's two-event default). Windows always reports explicit modifier state (`MOD_ALT` from `dwControlKeyState`), i.e. always `LT_INPUT_ALT`-style — the ESC-prefix distinction is a POSIX-terminal concern. `LT_INPUT_MOUSE` now emits the SGR (`?1000h`/`?1006h`) tracking-enable handshake on POSIX (disabled again on `lt_set_input_mode` clear and on `lt_shutdown`). POSIX behavior unit-tested in `tests/test_posix_input_parse.c` |

### Output mode

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_set_output_mode` | `lt_set_output_mode` | [x] | [x] | SGR emission branches on output mode through the shared `lt__emit_sgr` (`src/shared/sgr.c`), so Windows is no longer stores-only — it consumes the mode via the same path as POSIX (Windows real-terminal confirmation pending) |

### Print / send helpers

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_print` | `lt_print` | [x] | [x] | Shared (`src/shared/cell.c`): UTF-8 string into the back buffer, advancing by `lt_wcwidth` per codepoint — wide CJK/emoji take two columns, combining marks take zero (not placed; no extend-cell yet), non-printables render as `U+FFFD`. `\n` wraps to the start column of the next row, malformed UTF-8 → `U+FFFD`, off-buffer cells clipped |
| `tb_print_ex` | `lt_print_ex` | [x] | [x] | As `lt_print`, with `out_w` = widest line's column count. Tested in `tests/test_print.c` |
| `tb_printf` | `lt_printf` | [x] | [x] | `vsnprintf` into a 1 KiB buffer (truncates if longer) then `lt_print` |
| `tb_printf_ex` | `lt_printf_ex` | [x] | [x] | printf variant of `lt_print_ex`: `out_w` receives the widest line's column count. Shares a `vsnprintf` core with `lt_printf` (`src/shared/cell.c`); tested in `tests/test_print.c` |
| `tb_send` | `lt_send` | [x] | [x] | Shared (`src/shared/output.c`): raw bytes straight to the terminal via the platform output buffer + immediate flush, length-counted (embedded NULs allowed). For escape sequences libterm doesn't model. Tested in `tests/test_send.c` |
| `tb_sendf` | `lt_sendf` | [x] | [x] | `vsnprintf` into a 1 KiB buffer (truncates if longer) then `lt_send` |
| `tb_set_func` | `lt_set_func` | [—] | [—] | **Intentional divergence.** Custom parser-hook installer; termbox2 itself marks `tb_set_func` (and `TB_FUNC_EXTRACT_*`) deprecated, slated for removal in its 3.x. Mirroring it would add a function-pointer indirection into the input path for an API the upstream is dropping |

### UTF-8 helpers

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_utf8_char_length` | `lt_utf8_char_length` | [x] | [x] | Public wrapper exported over `lt__utf8_char_length` |
| `tb_utf8_char_to_unicode` | `lt_utf8_char_to_unicode` | [x] | [x] | Public wrapper exported over strict decode path |
| `tb_utf8_unicode_to_char` | `lt_utf8_unicode_to_char` | [x] | [x] | Public wrapper exported over `lt__utf8_encode` |

### Capability / introspection

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_last_errno` | `lt_last_errno` | [x] | [—] | Returns the `errno` captured at the most recent failing POSIX syscall (open/isatty/tcgetattr/tcsetattr behind `LT_ERR_INIT_OPEN`, read/select behind `LT_ERR_READ`/`LT_ERR_POLL`); 0 if nothing failed. Stored in `lt__g.last_errno`. Tested in `tests/test_last_errno.c`. Windows has no `errno`-based syscall layer, so the getter exists but always returns 0 there (`[—]`) |
| `tb_strerror` | `lt_strerror` | [x] | [x] | Implemented in `src/shared/errors.c` for **all 23** return codes (distinct messages + `"unknown error"` fallback); exhaustiveness + distinctness asserted in `tests/test_api.c` |
| `tb_has_truecolor` | `lt_has_truecolor` | [—] | [—] | **Intentional divergence.** termbox2's is a compile-time flag; libterm supersedes it with the runtime `lt_detect_color_depth` (below), which is strictly more informative (returns the actual color ceiling, not just a truecolor yes/no) |
| *(libterm addition)* | `lt_detect_color_depth` | [x] | [x] | Stateless runtime query: inspects `$COLORTERM` (`truecolor`/`24bit`) then `$TERM` (`*256color*` substring) and returns the terminal's color ceiling as an `LT_OUTPUT_*` mode (truecolor/256/normal). Pure standard-C `getenv` in `src/shared/output.c` — no platform code, safe to call before `lt_init`. Byte-exact logic asserted by hermetic `setenv`-driven test `tests/test_detect_color_depth.c` (harness POSIX-only; the function itself is platform-agnostic) |
| *(libterm addition)* | `lt_query_color` | [x] | [x] | The terminal's *actual* color for `LT_COLOR_DEFAULT_FG` / `LT_COLOR_DEFAULT_BG` / palette index 0..255, packed `0x00RRGGBB`. POSIX: real `OSC 10/11/4` round-trip (timeout-bounded, monotonic deadline; `rgb:` + URxvt `rgba:` replies; typed input preserved via the pushback ring; stray/late replies consumed by the decoder). Windows: native `GetConsoleScreenBufferInfoEx` color table — immediate; index > 15 → `LT_ERR_UNSUPPORTED_TERM`. Pty round-trip test `tests/test_color_query.c`, parser unit test `tests/test_color_parse.c`, Windows CSBI mapping test `tests/test_win_color_query.c`. Demoed in `examples/theme.c` |
| *(libterm addition)* | `lt_is_dark_background` | [x] | [x] | Queries the default background and thresholds its ITU-R BT.709 relative luminance: 1 dark / 0 light / negative `LT_ERR_*` passed through (recommended fallback: treat errors as "assume dark"). Same platform paths and tests as `lt_query_color`; demoed in `examples/theme.c` |
| `tb_has_egc` | `lt_has_egc` | [x] | [x] | Returns 1 — grapheme-cluster support is always built in (no compile-time opt-out, unlike termbox2's `TB_OPT_EGC`) |
| `tb_attr_width` | `lt_attr_width` | [x] | [x] | Returns `sizeof(lt_attr)` = 4. libterm has no compile-time attribute-width option (`lt_attr` is always `uint32_t`), so it's constant. Tested in `tests/test_wcwidth.c` |
| `tb_version` | `lt_version` | [x] | [x] | Returns `"0.1.3"` |
| `tb_iswprint` | `lt_iswprint` | [x] | [x] | Shared; non-zero when `lt_wcwidth(ch) >= 0`. Tested in `tests/test_wcwidth.c` |
| `tb_wcwidth` | `lt_wcwidth` | [x] | [x] | Shared (`src/shared/wcwidth.c`): 0 for combining/zero-width, 2 for wide CJK/emoji, 1 otherwise, -1 for control/non-printable. Markus Kuhn reference ranges (bisected combining table + wide-range tests); not locale-dependent. Consumed by `lt_print` for correct column advance |

---

## Public macros / constants

All renamed wholesale: every `TB_*` token becomes `LT_*`. The header `include/libterm/libterm.h` covers the subset libterm uses today; rows below show what is **declared and consumed** vs. what is still missing.

### Return codes (`TB_OK`, `TB_ERR*` → `LT_OK`, `LT_ERR*`)

| termbox2 | libterm | Declared | Used in code |
|---|---|---|---|
| `TB_OK` | `LT_OK` | [x] | [x] |
| `TB_ERR` | `LT_ERR` | [x] | [x] |
| `TB_ERR_NEED_MORE` | `LT_ERR_NEED_MORE` | [x] | [x] |
| `TB_ERR_INIT_ALREADY` | `LT_ERR_INIT_ALREADY` | [x] | [x] |
| `TB_ERR_INIT_OPEN` | `LT_ERR_INIT_OPEN` | [x] | [x] |
| `TB_ERR_MEM` | `LT_ERR_MEM` | [x] | [x] |
| `TB_ERR_NO_EVENT` | `LT_ERR_NO_EVENT` | [x] | [x] |
| `TB_ERR_NO_TERM` | `LT_ERR_NO_TERM` | [x] | [—] |
| `TB_ERR_NOT_INIT` | `LT_ERR_NOT_INIT` | [x] | [x] |
| `TB_ERR_OUT_OF_BOUNDS` | `LT_ERR_OUT_OF_BOUNDS` | [x] | [x] |
| `TB_ERR_READ` | `LT_ERR_READ` | [x] | [x] |
| `TB_ERR_RESIZE_IOCTL` | `LT_ERR_RESIZE_IOCTL` | [x] | [—] |
| `TB_ERR_RESIZE_PIPE` | `LT_ERR_RESIZE_PIPE` | [x] | [—] |
| `TB_ERR_RESIZE_SIGACTION` | `LT_ERR_RESIZE_SIGACTION` | [x] | [—] |
| `TB_ERR_POLL` | `LT_ERR_POLL` | [x] | [x] |
| `TB_ERR_TCGETATTR` | `LT_ERR_TCGETATTR` | [x] | [—] |
| `TB_ERR_TCSETATTR` | `LT_ERR_TCSETATTR` | [x] | [—] |
| `TB_ERR_UNSUPPORTED_TERM` | `LT_ERR_UNSUPPORTED_TERM` | [x] | [x] |
| `TB_ERR_RESIZE_WRITE` | `LT_ERR_RESIZE_WRITE` | [x] | [—] |
| `TB_ERR_RESIZE_POLL` | `LT_ERR_RESIZE_POLL` | [x] | [—] |
| `TB_ERR_RESIZE_READ` | `LT_ERR_RESIZE_READ` | [x] | [—] |
| `TB_ERR_RESIZE_SSCANF` | `LT_ERR_RESIZE_SSCANF` | [x] | [—] |
| `TB_ERR_CAP_COLLISION` | `LT_ERR_CAP_COLLISION` | [x] | [—] |

The [—] rows name POSIX-only syscall/terminfo failures; the producing call sites do not exist on Windows.
LT_ERR_UNSUPPORTED_TERM is produced on Windows by lt_get_fds and lt_query_color (palette index > 15).

### Event types (`TB_EVENT_*` → `LT_EVENT_*`)

| termbox2 | libterm | Status |
|---|---|---|
| `TB_EVENT_KEY` | `LT_EVENT_KEY` | declared, emitted (POSIX [~], Windows [x]) |
| `TB_EVENT_RESIZE` | `LT_EVENT_RESIZE` | declared; emitted on both platforms (Windows console resize event, POSIX SIGWINCH/self-pipe path) |
| `TB_EVENT_MOUSE` | `LT_EVENT_MOUSE` | declared; emitted on POSIX (SGR 1006 reports → button in `key`, 0-based coords in `x`/`y`, modifiers in `mod`) and on Windows (`MOUSE_EVENT_RECORD` → same fields, gated on `LT_INPUT_MOUSE`; tested in `tests/test_win_mouse.c`) |

### Keys (`TB_KEY_*` → `LT_KEY_*`)

Function/named keys (`F1`–`F12`, `INSERT`, `DELETE`, `HOME`, `END`, `PGUP`, `PGDN`, `ARROW_UP/DOWN/LEFT/RIGHT`, `ENTER`, `ESC`, `SPACE`, `BACKSPACE`, `BACKSPACE2`, `TAB`) are declared. Emission status:

| Key group | POSIX | Windows |
|---|---|---|
| Arrows + ESC | [x] | [x] |
| `ENTER` / `BACKSPACE` / `TAB` / `SPACE` | [x] | [x] | ENTER/TAB/BACKSPACE now report as key codes (termbox2 control-byte model); SPACE is a printable char (`ch == 0x20`) |
| F1–F12 | [x] (common xterm CSI/SS3 sequences) | [x] |
| `INSERT` / `DELETE` / `HOME` / `END` / `PGUP` / `PGDN` | [x] | [x] |
| Ctrl+letter (`LT_KEY_CTRL_A` … termbox2 set) | [x] | [x] | In **compat** mode (`LT_INPUT_COMPAT`) a standalone control byte (`0x00-0x1F`, `0x7F`) → `ev->key` = the byte, `ch == 0`, `mod == 0`, identically on both platforms (termbox2 model). In the **modern** model Ctrl+letter is disambiguated to `ch` = lowercase letter + `LT_MOD_CTRL` (`key == 0`): POSIX via the kitty CSI-u codepoint, Windows by reconstructing from the console virtual-key (`VK_I` stays distinct from `VK_TAB`), so `Ctrl+I` is no longer indistinguishable from Tab. **Windows disambiguation is best-effort** — it relies on the console reporting the letter virtual-key, which the classic console (conhost) does; **ConPTY / Windows Terminal pre-collapse `Ctrl+I`→Tab (`VK_TAB`) and `Ctrl+J`→Enter (`VK_RETURN`) at the virtual-key level**, so the letter is unrecoverable there, exactly as on a POSIX legacy terminal. `LT_KEY_CTRL_*` declared in `libterm.h`; POSIX byte mapping unit-tested in `tests/test_posix_input_parse.c`, Windows disambiguation in `tests/test_win_key.c` |
| Back-tab (Shift+Tab, `LT_KEY_BACK_TAB`) | [x] CSI `Z` → `LT_KEY_BACK_TAB` | [x] | POSIX parses `\x1b[Z` (per tmux + kitty); Windows maps `VK_TAB` + `SHIFT_PRESSED` → `LT_KEY_BACK_TAB`. Both unit-tested (`tests/test_posix_input_parse.c`, `tests/test_win_key.c`) |
| Modern key outcome (codepoint + modifiers + `ev->action`) | [x] via CSI-u (`\x1b[cp;mods u`) | [x] via `KEY_EVENT_RECORD` | POSIX parses the full kitty/fixterms CSI-u form via the shared decoder (`src/shared/keymap.c`): codepoint → `ev->ch`, `1 + bitmask` → `ev->mod`, event-type → `ev->action`. Windows reads the same outcome directly from the console record (no escape parsing): codepoint → `ev->ch`, `dwControlKeyState` → `ev->mod`, key-down/repeat/up → `LT_KEY_PRESS`/`REPEAT`/`RELEASE`. Both unit-tested (`tests/test_posix_input_parse.c`, `tests/test_win_key.c`) |
| Bare-modifier keys (`LT_KEY_LEFT_SHIFT` … `LT_KEY_RIGHT_SUPER`) | [x] kitty functional codes → `ev->key` | [x] modifier VKs → `ev->key` | **Deliberate divergence from termbox2** (no equivalent there). POSIX decodes the functional CSI-u codes (`src/shared/keymap.c`); Windows maps the modifier virtual-key codes (`VK_SHIFT/CONTROL/MENU/LWIN/RWIN/CAPITAL`, left/right via scan code + `ENHANCED_KEY`) → the corresponding `LT_KEY_*` with `ev->action`, modern model only. Legacy POSIX terminals send zero bytes for these; Windows and kitty report them. Unit-tested (`tests/test_posix_input_parse.c`, `tests/test_win_key.c`) |

### Modifiers (`TB_MOD_*` → `LT_MOD_*`)

| termbox2 | libterm | POSIX | Windows |
|---|---|---|---|
| `TB_MOD_ALT` | `LT_MOD_ALT` | [~] | [x] |
| `TB_MOD_CTRL` | `LT_MOD_CTRL` | [~] | [x] |
| `TB_MOD_SHIFT` | `LT_MOD_SHIFT` | [~] | [x] |
| `TB_MOD_MOTION` | `LT_MOD_MOTION` | [x] | [x] |

On **kitty-capable terminals** (negotiated by default), POSIX now delivers `LT_MOD_ALT`/`LT_MOD_CTRL`/`LT_MOD_SHIFT` on **all keys** — including bare-modifier presses, Shift+letter, and Ctrl+letter — via the CSI-u path in the shared decoder (`src/shared/keymap.c`). On **legacy-only terminals** these are still partial (`[~]`): modifiers ride only CSI-encoded keys (arrows, F-keys, nav) since the terminal folds Shift/Ctrl+letter before libterm sees it. POSIX mouse reports also set `LT_MOD_SHIFT`/`LT_MOD_CTRL`/`LT_MOD_ALT`/`LT_MOD_MOTION` from the SGR button-code modifier bits (4/8/16/32); `LT_MOD_MOTION` (drag) is producible only via mouse, hence `[x]` on POSIX.

### Colors (`TB_DEFAULT/BLACK/RED/…` → `LT_DEFAULT/BLACK/RED/…`)

Declared: `LT_DEFAULT`, `LT_BLACK`, `LT_RED`, `LT_GREEN`, `LT_YELLOW`, `LT_BLUE`, `LT_MAGENTA`, `LT_CYAN`, `LT_WHITE`, plus `LT_RGB(r,g,b)` (24-bit pack), `LT_HEX(0xRRGGBB)` (hex pack, high bits masked), and the `LT_HI_BLACK` sentinel. Emitted on **both platforms** via mode-aware SGR in the shared `lt__emit_sgr` (`src/shared/sgr.c`) — named (NORMAL), 8-bit palette index (256/216/grayscale), and 24-bit RGB (TRUECOLOR), with `LT_HI_BLACK` distinguishing the terminal default from real black. Emitted bytes asserted on POSIX in `tests/test_posix_sgr_output.c`; Windows runs the same shared code (real-terminal/byte verification pending).

### Attributes (`TB_BOLD/UNDERLINE/…` → `LT_BOLD/UNDERLINE/…`)

Declared: `LT_BOLD`, `LT_UNDERLINE`, `LT_REVERSE`, `LT_ITALIC`, `LT_BLINK`, `LT_DIM`, `LT_STRIKE` (bits 24-30, above the 24-bit color field). Emitted on **both platforms** via the shared SGR path (`lt__emit_sgr`, `src/shared/sgr.c`); POSIX byte-tested, Windows confirmation pending.

### Input modes (`TB_INPUT_*` → `LT_INPUT_*`)

Declared: `LT_INPUT_CURRENT`, `LT_INPUT_ESC`, `LT_INPUT_ALT`, `LT_INPUT_MOUSE`, `LT_INPUT_COMPAT`. Consumed: `lt_init` defaults to `LT_INPUT_ESC`; the POSIX input path branches on `LT_INPUT_ESC` vs `LT_INPUT_ALT` for Alt-combos (Windows uses explicit modifier state, always `LT_INPUT_ALT`-style). `LT_INPUT_MOUSE` is consumed on POSIX: setting it emits the SGR mouse tracking-enable handshake, and the input parser turns the resulting reports into `LT_EVENT_MOUSE` events (Windows mouse pending). `LT_INPUT_COMPAT` is a **deliberate libterm divergence from termbox2** (no equivalent flag there): by default libterm runs the *modern* model — control bytes are normalized to `ch`+`mod` and kitty enhancement is negotiated; setting `LT_INPUT_COMPAT` restores termbox2's control-byte-in-`key` semantics and suppresses kitty negotiation (popping the enhancement on switch).

### Output modes (`TB_OUTPUT_*` → `LT_OUTPUT_*`)

Declared: `LT_OUTPUT_CURRENT`, `LT_OUTPUT_NORMAL`, `LT_OUTPUT_256`, `LT_OUTPUT_216`, `LT_OUTPUT_GRAYSCALE`, `LT_OUTPUT_TRUECOLOR`. All emit correct SGR through the shared `lt__emit_sgr` (`src/shared/sgr.c`) — 24-bit truecolor and 8-bit palette indices — consumed identically on both platforms; byte-tested on POSIX in `tests/test_posix_sgr_output.c`, Windows confirmation pending.

### Function-hook ids (`TB_FUNC_*` → `LT_FUNC_*`)

Not declared — intentionally, alongside `lt_set_func` (deprecated upstream; see the Print / send helpers table).

### Version macros

`LT_VERSION_MAJOR` (0), `LT_VERSION_MINOR` (1), `LT_VERSION_PATCH` (0) are declared and returned by `lt_version`.

---

## Public types

| termbox2 | libterm | Status |
|---|---|---|
| `uintattr_t` | `lt_attr` (`uint32_t`) | declared and used everywhere; layout: color bits 0-23, attribute bits 24-30, `LT_HI_BLACK` bit 31 |
| *(termbox2 uses `uint32_t` directly)* | `lt_uchar` (`uint32_t`) | libterm-internal alias for codepoints |
| `struct tb_cell` | `struct lt_cell` (`ch`, `fg`, `bg`, `_reserved`) | Fixed 16 bytes (SIMD-aligned). EGC is handled differently from termbox2: instead of per-cell `ech`/`nech`/`cech` heap pointers, `_reserved` doubles as a grapheme-cluster id into an out-of-line content-deduped table (`src/shared/egc.c`), keeping the cell POD and the byte-equality diff intact |
| `struct tb_event` | `struct lt_event` (`type`, `mod`, `key`, `ch`, `w`, `h`, `x`, `y`, `action`) | declared; matches termbox2 layout plus a libterm-added `action` field. **Deliberate divergence:** `action` reports `LT_KEY_PRESS`/`LT_KEY_REPEAT`/`LT_KEY_RELEASE` (sourced from the kitty event-type sub-parameter); termbox2 has no press/repeat/release distinction. Defaults to press on legacy terminals |

---

## What is verified end-to-end today

A feature is listed here only if it has been observed working on a real terminal and is not blocked by a known correctness bug.

| Capability | POSIX | Windows |
|---|---|---|
| `lt_init` / `lt_shutdown` round-trip without leaking handles | [x] raw mode + alt-screen + SIGWINCH self-pipe lifecycle; auto-tested headlessly via `lt_init_fd` + `openpty` (`tests/test_pty_lifecycle.c`) | [x] saves and restores both console modes |
| Console size queried from kernel (not env) | [x] `ioctl(TIOCGWINSZ)` | [x] `csbi.srWindow`-based viewport size |
| `lt_clear` zeroes the back buffer | [x] | [x] |
| `lt_set_cell` writes a codepoint into the back buffer | [x] | [x] |
| `lt_present` actually paints the terminal | [x] buffered ANSI output | [x] cursor jump (only on discontinuity) + UTF-8 emit per changed cell, buffered `WriteFile` |
| ASCII char keys via `lt_poll_event` / `lt_peek_event` | [~] UTF-8 assembly path now active with `U+FFFD` fallback; still needs broader parity validation | [x] disambiguated via `KEY_EVENT_RECORD` |
| Named keys (F1–F12, arrows, Home/End/PgUp/PgDn, Ins/Del) | [x] common xterm CSI/SS3 coverage (terminal-dependent beyond that set) | [x] full set via `plat_keys.c` |
| Modifier bits in `ev->mod` | [~] CSI modifier suffixes mapped for POSIX escape-key families; coverage is not yet universal | [x] from `dwControlKeyState` on every key; `ev->action` press/repeat/release reported in the modern model |
| UTF-8 input round-trip (BMP + supplementary) | [x] multi-byte assembly + strict decode with `U+FFFD` fallback; malformed sequences now **resync** (a non-continuation byte after a bad lead is replayed as its own event, not swallowed). End-to-end pty test `tests/test_posix_input_utf8.c` + decode boundary tests in `tests/test_utf8.c` | [x] surrogate pairs combined before emit; latch cleared on every non-completing return path |
| UTF-8 output round-trip in render path | [x] `lt__utf8_encode` path active | [x] `lt__utf8_encode` writes 1–4 bytes |
| `LT_EVENT_RESIZE` delivered exactly once per visible-size change | [x] validated end-to-end in `tests/test_resize_event.c`: exactly-once per visible change, burst coalescing to the settled size, no-op suppression, and a full-repaint ghost-regression byte assertion (resize sets `force_repaint` via the shared `lt__handle_resize`) | [x] `WINDOW_BUFFER_SIZE_EVENT` filtered for spurious events |
| Diff-based `lt_present` (skip unchanged cells) | [x] shared path | [x] shared path |
| Cursor-position cache (skip jump on natural advance) | [x] shared path | [x] shared path |
| Alt-screen UX (prior scrollback preserved on exit) | [x] `\x1b[?1049h` / `\x1b[?1049l` | [x] `\x1b[?1049h` on init / `\x1b[?1049l` on shutdown |
| Hand-rolled int-to-decimal in render path (no `snprintf`) | [x] | [x] `lt__plat_move_cursor` writes digits directly |
| Bench harness (`bench/bench_present.c`) | [ ] | [x] three scenarios (no-change / one-cell / full-repaint) timed via QPC |
| SGR / color emission | [x] byte-tested via pty (`tests/test_posix_sgr_output.c`) | [x] shared `lt__emit_sgr` (`src/shared/sgr.c`); colors visually confirmed in Windows Terminal via the bench SGR workloads (`bench/bench_present.c`, real `WriteFile` output) + automated byte test (`tests/test_win_sgr_output.c`) |
| Runtime color-depth detection (`lt_detect_color_depth`) | [x] `$COLORTERM`/`$TERM` → `LT_OUTPUT_*` ceiling; hermetic `setenv` test (`tests/test_detect_color_depth.c`), wired into `examples/truecolor.c` | [x] same standard-C path (test harness POSIX-only) |
| Color querying (`lt_query_color`, `lt_is_dark_background`) | [x] OSC 10/11/4 round-trip with monotonic deadline; `rgb:`/`rgba:` reply parser; typed input preserved (pushback ring); stray replies consumed by the decoder. Pty test `tests/test_color_query.c` + parser test `tests/test_color_parse.c`; demo `examples/theme.c` | [x] native `GetConsoleScreenBufferInfoEx` color table (immediate; palette > 15 → `LT_ERR_UNSUPPORTED_TERM`); CSBI mapping test `tests/test_win_color_query.c` (runs on Windows CI; MinGW cross-compiled) |
| Runtime SIMD dispatch (`LIBTERM_SIMD=auto`) | [x] all arch backends compiled + constructor-selected at startup (x86_64: cpuid scalar→AVX2→AVX-512; aarch64: hwcap NEON→SVE; riscv64: hwcap scalar→RVV); explicit `LIBTERM_SIMD=<backend>` keeps the static single-TU build; per-backend correctness sweep (`tests/test_simd_diff.c`), dispatcher sanity (`tests/test_simd_dispatch.c`), per-backend emitted-ISA asserts | [x] same dispatch path under MinGW (`__builtin_cpu_supports` + constructor); MSVC keeps legacy static pick (no constructor attribute) |
| Release artifacts (`release.yml`) | [x] 6-target matrix on `release: published` (+ `workflow_dispatch` backfill): native lanes re-test in Release mode, riscv64/MinGW cross lanes build-only; install-tree archives + `SHA256SUMS` attached via `gh release upload --clobber`; tag-vs-CMake-VERSION guard | [x] windows-x86_64 zip (MinGW cross; DLL in `bin/`) |
| Mouse events | [~] SGR (1006) reports parsed into `LT_EVENT_MOUSE` (button/wheel/release, 0-based coords, shift/ctrl/alt/motion mods); tracking enabled via `lt_set_input_mode(LT_INPUT_MOUSE)`, disabled on shutdown. Parser unit-tested incl. malformed/overflow rejection (`tests/test_posix_input_parse.c`); live-terminal click verification still pending | [~] `MOUSE_EVENT_RECORD` parsed into `LT_EVENT_MOUSE` (button/wheel/release, viewport-relative 0-based coords, shift/ctrl/alt/motion mods); `ENABLE_MOUSE_INPUT` toggled via the `lt__plat_set_mouse` hook. Mapper unit-tested (`tests/test_win_mouse.c`); live-terminal click verification still pending |

---

## Known blockers

These are the things that, if fixed, would move the largest number of `[~]` rows above to `[x]`.

1. ~~**Windows SGR/color emission.**~~ **Resolved.** SGR emission was extracted into the shared `src/shared/sgr.c` (`lt__emit_sgr` / `lt__render_run`); both platforms now run it. Colors were visually confirmed in Windows Terminal (bench SGR workloads, real `WriteFile` output) and are guarded by an automated Windows byte test (`tests/test_win_sgr_output.c`) alongside the POSIX pty test.
2. **POSIX modifier semantics are partial.** CSI modifier suffixes are mapped for the escape-key families, and the modern CSI-u encoding (`\x1b[cp;mods u`, fixterms/kitty) is parsed so Ctrl/Shift/Alt+letter now arrive unambiguously where the terminal emits it. Legacy ambiguous cases (e.g. terminals that send bare control bytes without CSI-u) remain inherently lossy; behavior is not yet universal across every terminal.
3. ~~**POSIX UTF-8 input semantics need parity hardening.**~~ **Resolved.** Multi-byte assembly + strict decode with `U+FFFD` fallback now resync correctly (no byte-swallowing on malformed input), covered end-to-end by a pty test (`tests/test_posix_input_utf8.c`) and decode boundary tests. Remaining input work is general cross-terminal escape-sequence coverage (poll/peek), not UTF-8.
4. **Public API surface complete on POSIX, modulo intentional divergences.** Every termbox2 public function is implemented except three deliberate omissions, now marked `[—]`: `lt_init_rwfd` (single-fd by design), `lt_set_func` (deprecated upstream), and `lt_has_truecolor` (superseded by `lt_detect_color_depth`). Implemented this round: `lt_init_file`, `lt_printf_ex`, `lt_attr_width` — plus, earlier, `lt_init_fd`, the print/send helpers, `lt_get_cell`, `lt_get_fds`, `lt_last_errno`, `lt_wcwidth`/`lt_iswprint`, and the grapheme-cluster set.
5. **Output-mode parity now shares one code path.** Both platforms consume `lt_set_output_mode` through the shared `lt__emit_sgr`, so the previous POSIX-only / Windows-stores-only skew is gone; remaining work is the Windows verification noted in blocker #1.
6. **macOS pty-test parity.** 14/16 pty-based tests hang or fail on macOS CI runners (Linux is fully green): byte-asserting tests can block mid-`lt_present` on the small macOS pty buffer; a near-universal hang persists even in drain-correct tests (suspected `tcsetattr(TCSAFLUSH)` waiting for master drain during `lt_shutdown` — unconfirmed); `test_posix_input_frag`'s 40ms-gap-vs-50ms-grace margin is too tight for shared runners. The pty tests are Apple-gated in tests/CMakeLists.txt; `test_pty_lifecycle` (with `tests/pty_drain.h` autodrain) stays enabled as the canary. Needs a dedicated diagnosis slice (instrumented CI runs or a local Mac).

---

## Planned enhancements (beyond termbox2 parity)

Features that go past the termbox2 API surface but are wanted for libterm. Not required for v1.0 (which is termbox2 parity), tracked here so the design intent isn't lost.

### Kitty keyboard protocol (progressive enhancement)

> **Status: shipped.** The progressive enhancement is now implemented and negotiated by default on POSIX (`lt__plat_kitty_enable` pushes `CSI > 11 u` on init, pops `CSI < u` on shutdown; Windows is a no-op). The full kitty CSI-u form is parsed in the shared decoder (`src/shared/keymap.c`) with bare-modifier, key-release, and key-repeat support (`lt_event.action`). Opt out with `LT_INPUT_COMPAT`, which suppresses negotiation and restores termbox2 control-byte semantics. Legacy / non-supporting terminals degrade gracefully to the existing fixed-sequence + CSI parsing with no regression. (Kitty negotiation byte-sequence is verified by `tests/test_kitty_negotiation.c` via pty capture.) The original design notes below are kept for context.

**Problem.** Legacy terminal input is lossy for modifiers, and it is the *terminal* encoding — not libterm — that drops the information:

- A **bare modifier** press (Ctrl / Shift / Alt alone) sends **zero bytes**, so it can never be reported.
- **Shift+letter** is folded into an uppercase character: the terminal sends `'A'` with no modifier bit (verified — `lt_peek_event` returns `key=0x00 ch=0x41 mod=0x00`).
- **Ctrl+letter** collapses to a control byte (`Ctrl+S` → `0x13`), indistinguishable from `Tab`/`Enter`/`Backspace` for `Ctrl+I/M/H`.
- There are no key **release** or **repeat** events.

Modifiers are reported today *only* on CSI-encoded keys (arrows, F-keys, nav): `Shift+Up` → `mod=LT_MOD_SHIFT`, `Ctrl+Up` → `mod=LT_MOD_CTRL`, `Alt+x` → `mod=LT_MOD_ALT` (the last via `LT_INPUT_ALT`). The `LT_MOD_*` rows for POSIX are `[ ]` (see Modifiers table) precisely because letters can't carry them in legacy mode.

**Solution.** Implement the [kitty keyboard protocol](external/kitty/docs/keyboard-protocol.rst) progressive enhancement (a superset of fixterms / `CSI u`). Supporting terminals then report every modifier — including bare presses, releases, and Shift+letter — as unambiguous `CSI … u` escape codes.

**Work involved:**
- **Negotiation.** On `lt_init` (POSIX), push the desired flags with `CSI > <flags> u` and pop on `lt_shutdown` with `CSI < u`; optionally query support first with `CSI ? u`. Fall back silently to the current legacy parsing if the terminal doesn't respond — no regression on non-supporting terminals.
- **Flags.** `0x1` disambiguate escape codes, `0x2` report event types (press/repeat/release), `0x4` report alternate keys, `0x8` report all keys as escape codes, `0x10` report associated text. A sensible default is `0x1 | 0x2 | 0x8`.
- **Parsing.** The `CSI <cp>;<mods> u` report parser already exists (`tests/test_posix_input_parse.c`); extend it to the full form `CSI <cp>[:alt];<mods>[:event-type];<text> u` — decode the event-type sub-parameter (1 press / 2 repeat / 3 release) and the associated-text field.
- **API surface (new).** Decide how to surface key **release/repeat** and **bare modifier** events: either a new event-action field on `struct lt_event` (e.g. `press`/`repeat`/`release`) or a distinct event type. Either is a deliberate addition beyond termbox2 — note it as a libterm divergence when added.
- **Input-mode flag.** Likely a new `LT_INPUT_*` bit (e.g. `LT_INPUT_KITTY`) to opt in, keeping the legacy two-event Esc/Alt model the default for drop-in termbox2 compatibility.

**Terminals:** kitty, foot, ghostty, WezTerm, recent xterm; Alacritty partial. The older `xterm` `modifyOtherKeys` (`CSI > 4 ; 2 m`) is a partial alternative (more combos, but still no bare-modifier or release events) and could be a secondary fallback.

**Payoff.** Moves the POSIX `LT_MOD_ALT/CTRL/SHIFT` rows toward `[x]` for *all* keys (not just CSI families), enables bare-modifier and Shift+letter detection, adds key-release/repeat, and resolves the "inherently lossy" caveat in Known blocker #2. `examples/kbd.c` (the on-screen keyboard + live event inspector) is the natural development harness — once libterm emits the bits, its inspector and modifier caps light up for every combination.

### Color querying + theming (kitty-enhanced)

> **Status: slice 1 shipped — `lt_query_color` + `lt_is_dark_background`.** POSIX does the OSC 10/11/4 round-trip (timeout-bounded, typed input preserved via a raw-byte pushback ring); Windows answers natively from `GetConsoleScreenBufferInfoEx` (palette indexes above 15 unsupported). Spec: `docs/specs/2026-06-04-color-query-design.md`. Remaining slices: mode 2031 theme-changed events and the kitty OSC 21 color stack.

**Idea.** Let an app learn the terminal's actual colors so it can detect a **light vs dark background** and theme itself accordingly. The valuable core is *not* kitty-specific: querying the default foreground/background and the palette is plain `OSC 10` / `11` / `4` with a `?`, supported broadly (xterm, kitty, foot, alacritty, WezTerm, ghostty, iTerm2). Kitty's extended `OSC 21` layers on top as a progressive enhancement — batch query/set, special colors, and a push/pop **color stack** to recolor the terminal and restore it cleanly on exit. Same shape as the keyboard work: portable base, kitty-enhanced.

**API surface (slice 1 shipped: queries + dark-bg; rest planned).**
- *Shipped:* query fg / bg / palette index with a timeout (replies are X11-style `rgb:rrrr/gggg/bbbb`, 16-bit per channel, terminated by BEL *or* ST — terminals differ).
- *Shipped:* `lt_is_dark_background()` convenience (luminance threshold on the bg query) — what most callers actually want.
- *Planned:* a **theme-changed event**: mode `2031` / `CSI ? 996 n` is the newer cross-terminal "color scheme notification" mechanism (kitty, ghostty, foot). `OSC 11` answers point-in-time; mode 2031 delivers *change events* when the user flips OS theme mid-session — falls out almost free once the query path exists, and libterm already has the event loop to surface it.
- *Planned:* color *setting* exposed only via the kitty color stack (push/pop), gated on detection — arbitrary palette writes are how apps leave terminals in a broken state, so not offering them is a feature.

**Notes / open questions.**
- The query round-trip machinery (emit the OSC, read the reply off the input stream with a deadline, stash interleaved input for replay, consume stray/late replies in the decoder) was built by slice 1 in `lt__plat_query_color` / the pushback ring — and is exactly what the mode-2031 slice will reuse. (The kitty *keyboard* negotiation, by contrast, is a blind push with no reply reading.)
- **Windows** ended up better than the anticipated "documented caveat": no OSC at all — the console color table answers natively on every Windows version (only palette indexes above 15 are out of reach, documented in the README).
- **Multiplexers.** tmux answers `OSC 10`/`11` itself (with its own idea of colors) — documented in the README. `OSC 21` passthrough is unreliable; that caveat lands with the color-stack slice.
- Complements `lt_detect_color_depth` (which reports *how many* colors, but nothing about light/dark or the actual palette).
- Exact escape codes and the public API surface for the shipped slice are pinned in `docs/specs/2026-06-04-color-query-design.md`; the remaining slices (mode 2031 events, kitty color stack) are still to be designed.

---

## Release & distribution

Consumption channels, tracked like the API tables above: a channel counts as
`[x]` only when it works end-to-end *and* CI guards it.

| Channel | Status | Notes |
|---|---|---|
| `find_package(Libterm)` (installed tree) | [x] | install/export smoke-tested in default + shared-only configurations (`tests/smoke_install.sh`) |
| `FetchContent` / `add_subdirectory` | [x] | clean by default — one static-lib target, everything else top-level-only opt-in; guarded by `fetchcontent-smoke` on ubuntu/windows/macos |
| pkg-config (`lib/pkgconfig/libterm.pc`) | [x] | relocatable (`${pcfiledir}`-relative prefix, survives tarball moves); CI-guarded incl. a moved-prefix pass |
| Manual build (no build system) | [x] | README "Without CMake" `cc`+`ar` scalar recipe; CI executes it verbatim (`tests/smoke_manual_build.sh`) |
| Prebuilt archives | [x] | 6 targets (linux x86_64/aarch64/riscv64, windows x86_64 MinGW, macos arm64/x86_64) + `SHA256SUMS` per release; tag-vs-CMake-version guard; rebuildable per tag via `workflow_dispatch` |
| vcpkg | [ ] | next channel — name is free in the registry (checked 2026-06-06); overlay-port vs direct microsoft/vcpkg submission undecided; v0.1.0 is the reference tag |
| AUR | [ ] | a PKGBUILD; audience overlaps heavily with terminal tooling |
| Fedora COPR | [ ] | self-service personal repo — fine pre-1.0, unlike official Fedora |
| Ubuntu PPA (Launchpad) | [ ] | self-service personal repo — fine pre-1.0, unlike official Debian/Ubuntu |
| Homebrew | [ ] | personal tap first (builds from source — sidesteps the unsigned-binary caveat); homebrew-core has notability thresholds, revisit at 1.0 |
| Official distro repos (Debian / Fedora / homebrew-core) | [—] | deliberately deferred until 1.0 — they want API/ABI stability and pre-1.0 minors may break API |

**Open questions.**
- macOS binary signing/notarization: currently unsigned with the documented
  `xattr` workaround; revisit if macOS adoption grows (a tap or source builds
  make it moot).
- Single-file amalgamation (scalar-only `libterm.c`/`.h`): the strongest
  from-source story in this niche (termbox2 is single-header), but nontrivial
  against the platform split and TU-local statics — deferred, unscheduled.

---

## Out of scope for libterm (intentional divergence from termbox2)

- Header-only distribution. libterm ships as a compiled library by design; `compat/termbox2.h` (planned) will provide the drop-in macro layer.
- POSIX shim on Windows. Windows uses `ReadConsoleInputW` and Win32 console handles directly; no cygwin/msys assumptions.
- GPU-accelerated render path. refterm informs the *escape-emission* hot path (run coalescing, glyph→bytes cache); a Direct2D/Vulkan atlas is explicitly not a goal.

---

## Definition of "complete" (v1.0)

1. Every row in the public-functions tables above is `[x]` on both platforms.
2. Every declared `LT_*` macro is consumed by the implementation (no dead constants).
3. POSIX and Windows produce identical `lt_event` sequences for the same physical key, mouse, and resize input.
4. `lt_present` is diff-based and emits zero bytes when no cell has changed.
5. UTF-8 input and output round-trip correctly for 1–4-byte sequences and grapheme clusters.
6. All four output color modes render correctly on Windows Terminal, xterm, tmux, macOS Terminal, and the Linux console. SGR emission is implemented as one shared code path (`src/shared/sgr.c`) consumed by both platforms, with POSIX byte-correctness tested (`tests/test_posix_sgr_output.c`); cross-terminal visual confirmation and an automated Windows byte test remain.
7. Sanitizer CI is green (Linux gcc + clang, Windows MSVC); fuzzer runs an hour without crashes; valgrind reports no leaks.
8. `compat/termbox2.h` drop-in layer exists and a non-trivial example (small text editor) builds against it.
