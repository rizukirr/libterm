# libterm — Roadmap

Tracks the full termbox2 public API, renamed from the `tb_` / `TB_` prefix to `lt_` / `LT_`, and the current implementation status of each symbol on POSIX and Windows.

A symbol counts as **working** only when it actually performs the documented effect end-to-end on a real terminal — declarations and no-op stubs do not count. POSIX and Windows are tracked independently.

Legend: `[x]` working · `[~]` partial / stubbed · `[ ]` not implemented · `[—]` not applicable

---

## Public functions

### Lifecycle

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_init` | `lt_init` | [~] | [x] | POSIX `lt__plat_init` is a no-op (no termios raw-mode entry yet). Windows enters raw mode + alt-screen (`\x1b[?1049h`) so prior scrollback is preserved across the libterm session |
| `tb_init_file` | `lt_init_file` | [ ] | [ ] | Not declared in `libterm.h` |
| `tb_init_fd` | `lt_init_fd` | [ ] | [—] | Not declared |
| `tb_init_rwfd` | `lt_init_rwfd` | [ ] | [—] | Not declared |
| `tb_shutdown` | `lt_shutdown` | [~] | [x] | POSIX shutdown does not restore termios (init never entered raw mode). Windows leaves alt-screen (`\x1b[?1049l`) after cursor-show + flush, then restores both console modes |

### Screen geometry & rendering

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_width` | `lt_width` | [x] | [x] | Reads cached `lt__g.width` |
| `tb_height` | `lt_height` | [x] | [x] | Reads cached `lt__g.height` |
| `tb_clear` | `lt_clear` | [x] | [x] | Clears back buffer with current clear attrs |
| `tb_set_clear_attrs` | `lt_set_clear_attrs` | [x] | [x] | |
| `tb_present` | `lt_present` | [~] | [x] | Shared diff loop skips cells where `back == front` (3-field equality). Windows path also caches last-emitted cursor position (`lt__g.cur_x` / `cur_y`) so adjacent changed cells skip the cursor jump, and drains the entire frame in one `WriteFile`. POSIX `lt__plat_render_cell` / `move_cursor` / `flush` are still no-ops, so nothing reaches the screen there |
| `tb_invalidate` | `lt_invalidate` | [ ] | [ ] | Not declared |
| `tb_set_cursor` | `lt_set_cursor` | [~] | [x] | POSIX `lt__plat_move_cursor` is a no-op. Windows uses a hand-rolled int-to-decimal writer for `\x1b[r;cH` formatting (no `snprintf`/`printf`-family in the render hot path) |
| `tb_hide_cursor` | `lt_hide_cursor` | [~] | [x] | POSIX `lt__plat_hide_cursor` is a no-op |
| *(libterm addition)* | `lt_show_cursor` | [~] | [x] | Mirror of `lt_hide_cursor`; POSIX path is a no-op |
| `tb_set_cell` | `lt_set_cell` | [x] | [x] | Bounds-checked write into back buffer |
| `tb_set_cell_ex` | `lt_set_cell_ex` | [ ] | [ ] | Not declared (multi-codepoint EGC variant) |
| `tb_extend_cell` | `lt_extend_cell` | [ ] | [ ] | Not declared |
| `tb_get_cell` | `lt_get_cell` | [ ] | [ ] | Not declared |

### Input

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_poll_event` | `lt_poll_event` | [~] | [x] | POSIX reads single bytes only; ESC sequences, named keys beyond arrows, modifiers, UTF-8 assembly all missing |
| `tb_peek_event` | `lt_peek_event` | [~] | [x] | Same caveats as `lt_poll_event` |
| `tb_get_fds` | `lt_get_fds` | [ ] | [—] | Not declared |
| `tb_set_input_mode` | `lt_set_input_mode` | [~] | [~] | Stores the flag in `lt__g`; nothing in the input path reads it yet |

### Output mode

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_set_output_mode` | `lt_set_output_mode` | [~] | [~] | Stores the flag; SGR emission is not yet driven by it (M3 work) |

### Print / send helpers

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_print` | `lt_print` | [ ] | [ ] | |
| `tb_print_ex` | `lt_print_ex` | [ ] | [ ] | |
| `tb_printf` | `lt_printf` | [ ] | [ ] | |
| `tb_printf_ex` | `lt_printf_ex` | [ ] | [ ] | |
| `tb_send` | `lt_send` | [ ] | [ ] | |
| `tb_sendf` | `lt_sendf` | [ ] | [ ] | |
| `tb_set_func` | `lt_set_func` | [ ] | [ ] | Custom `extract_event` / `extract_pre` / `extract_post` hook |

### UTF-8 helpers

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_utf8_char_length` | `lt_utf8_char_length` | [~] | [~] | Implemented internally as `lt__utf8_char_length`; not exposed as public `lt_*` symbol yet |
| `tb_utf8_char_to_unicode` | `lt_utf8_char_to_unicode` | [~] | [~] | Implemented internally as `lt__utf8_decode`; not exposed |
| `tb_utf8_unicode_to_char` | `lt_utf8_unicode_to_char` | [~] | [~] | Implemented internally as `lt__utf8_encode`; not exposed |

### Capability / introspection

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_last_errno` | `lt_last_errno` | [ ] | [ ] | |
| `tb_strerror` | `lt_strerror` | [ ] | [ ] | Declared in `libterm.h`; no implementation linked |
| `tb_has_truecolor` | `lt_has_truecolor` | [ ] | [ ] | |
| `tb_has_egc` | `lt_has_egc` | [ ] | [ ] | |
| `tb_attr_width` | `lt_attr_width` | [ ] | [ ] | |
| `tb_version` | `lt_version` | [x] | [x] | Returns `"0.1.0"` |
| `tb_iswprint` | `lt_iswprint` | [ ] | [ ] | |
| `tb_wcwidth` | `lt_wcwidth` | [ ] | [ ] | |

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
| `TB_ERR_INIT_OPEN` | `LT_ERR_INIT_OPEN` | [x] | [ ] |
| `TB_ERR_MEM` | `LT_ERR_MEM` | [x] | [x] |
| `TB_ERR_NO_EVENT` | `LT_ERR_NO_EVENT` | [x] | [x] |
| `TB_ERR_NO_TERM` | `LT_ERR_NO_TERM` | [x] | [ ] |
| `TB_ERR_NOT_INIT` | `LT_ERR_NOT_INIT` | [x] | [x] |
| `TB_ERR_OUT_OF_BOUNDS` | `LT_ERR_OUT_OF_BOUNDS` | [x] | [x] |
| `TB_ERR_READ` | `LT_ERR_READ` | [x] | [x] |
| `TB_ERR_RESIZE_IOCTL` | `LT_ERR_RESIZE_IOCTL` | [x] | [ ] |
| `TB_ERR_RESIZE_PIPE` | `LT_ERR_RESIZE_PIPE` | [x] | [ ] |
| `TB_ERR_RESIZE_SIGACTION` | `LT_ERR_RESIZE_SIGACTION` | [x] | [ ] |
| `TB_ERR_POLL` | `LT_ERR_POLL` | [x] | [ ] |
| `TB_ERR_TCGETATTR` | `LT_ERR_TCGETATTR` | [x] | [ ] |
| `TB_ERR_TCSETATTR` | `LT_ERR_TCSETATTR` | [x] | [ ] |
| `TB_ERR_UNSUPPORTED_TERM` | `LT_ERR_UNSUPPORTED_TERM` | [x] | [ ] |
| `TB_ERR_RESIZE_WRITE` | `LT_ERR_RESIZE_WRITE` | [x] | [ ] |
| `TB_ERR_RESIZE_POLL` | `LT_ERR_RESIZE_POLL` | [x] | [ ] |
| `TB_ERR_RESIZE_READ` | `LT_ERR_RESIZE_READ` | [x] | [ ] |
| `TB_ERR_RESIZE_SSCANF` | `LT_ERR_RESIZE_SSCANF` | [x] | [ ] |
| `TB_ERR_CAP_COLLISION` | `LT_ERR_CAP_COLLISION` | [x] | [ ] |

### Event types (`TB_EVENT_*` → `LT_EVENT_*`)

| termbox2 | libterm | Status |
|---|---|---|
| `TB_EVENT_KEY` | `LT_EVENT_KEY` | declared, emitted (POSIX [~], Windows [x]) |
| `TB_EVENT_RESIZE` | `LT_EVENT_RESIZE` | declared; only Windows emits it today |
| `TB_EVENT_MOUSE` | `LT_EVENT_MOUSE` | declared; not emitted on either platform |

### Keys (`TB_KEY_*` → `LT_KEY_*`)

Function/named keys (`F1`–`F12`, `INSERT`, `DELETE`, `HOME`, `END`, `PGUP`, `PGDN`, `ARROW_UP/DOWN/LEFT/RIGHT`, `ENTER`, `ESC`, `SPACE`, `BACKSPACE`, `BACKSPACE2`, `TAB`) are declared. Emission status:

| Key group | POSIX | Windows |
|---|---|---|
| Arrows + ESC | [x] | [x] |
| `ENTER` / `BACKSPACE` / `TAB` / `SPACE` | [~] (raw byte only) | [x] |
| F1–F12 | [ ] | [x] |
| `INSERT` / `DELETE` / `HOME` / `END` / `PGUP` / `PGDN` | [ ] | [x] |
| Ctrl+letter (`TB_KEY_CTRL_A` … termbox2 set) | [ ] | [ ] |

### Modifiers (`TB_MOD_*` → `LT_MOD_*`)

| termbox2 | libterm | POSIX | Windows |
|---|---|---|---|
| `TB_MOD_ALT` | `LT_MOD_ALT` | [ ] | [x] |
| `TB_MOD_CTRL` | `LT_MOD_CTRL` | [ ] | [x] |
| `TB_MOD_SHIFT` | `LT_MOD_SHIFT` | [ ] | [x] |
| `TB_MOD_MOTION` | `LT_MOD_MOTION` | [ ] | [ ] |

### Colors (`TB_DEFAULT/BLACK/RED/…` → `LT_DEFAULT/BLACK/RED/…`)

Declared: `LT_DEFAULT`, `LT_BLACK`, `LT_RED`, `LT_GREEN`, `LT_YELLOW`, `LT_BLUE`, `LT_MAGENTA`, `LT_CYAN`, `LT_WHITE`. **Not yet emitted as SGR** on either platform — `lt__plat_render_cell` writes the codepoint without color attributes.

### Attributes (`TB_BOLD/UNDERLINE/…` → `LT_BOLD/UNDERLINE/…`)

Declared: `LT_BOLD`, `LT_UNDERLINE`, `LT_REVERSE`, `LT_ITALIC`, `LT_BLINK`, `LT_DIM`, `LT_STRIKE`. Not emitted.

### Input modes (`TB_INPUT_*` → `LT_INPUT_*`)

Declared: `LT_INPUT_CURRENT`, `LT_INPUT_ESC`, `LT_INPUT_ALT`, `LT_INPUT_MOUSE`. `lt_set_input_mode` records the value but no input path branches on it yet.

### Output modes (`TB_OUTPUT_*` → `LT_OUTPUT_*`)

Declared: `LT_OUTPUT_CURRENT`, `LT_OUTPUT_NORMAL`, `LT_OUTPUT_256`, `LT_OUTPUT_216`, `LT_OUTPUT_GRAYSCALE`, `LT_OUTPUT_TRUECOLOR`. Stored but not consumed.

### Function-hook ids (`TB_FUNC_*` → `LT_FUNC_*`)

Not declared. Dependent on `lt_set_func`.

### Version macros

`LT_VERSION_MAJOR` (0), `LT_VERSION_MINOR` (1), `LT_VERSION_PATCH` (0) are declared and returned by `lt_version`.

---

## Public types

| termbox2 | libterm | Status |
|---|---|---|
| `uintattr_t` | `lt_attr` (`uint32_t`) | declared and used everywhere |
| *(termbox2 uses `uint32_t` directly)* | `lt_uchar` (`uint32_t`) | libterm-internal alias for codepoints |
| `struct tb_cell` | `struct lt_cell` (`ch`, `fg`, `bg`) | declared; termbox2's optional `ech`/`nech`/`cech` (EGC) fields not present |
| `struct tb_event` | `struct lt_event` (`type`, `mod`, `key`, `ch`, `w`, `h`, `x`, `y`) | declared; matches termbox2 layout |

---

## What is verified end-to-end today

A feature is listed here only if it has been observed working on a real terminal and is not blocked by a known correctness bug.

| Capability | POSIX | Windows |
|---|---|---|
| `lt_init` / `lt_shutdown` round-trip without leaking handles | [~] init is a no-op stub | [x] saves and restores both console modes |
| Console size queried from kernel (not env) | [ ] `lt__plat_get_size` returns hardcoded zeros / TODO | [x] `csbi.srWindow`-based viewport size |
| `lt_clear` zeroes the back buffer | [x] | [x] |
| `lt_set_cell` writes a codepoint into the back buffer | [x] | [x] |
| `lt_present` actually paints the terminal | [ ] no bytes leave the process | [x] cursor jump (only on discontinuity) + UTF-8 emit per changed cell, single `WriteFile` per frame |
| ASCII char keys via `lt_poll_event` / `lt_peek_event` | [~] single-byte only; no ESC parser | [x] disambiguated via `KEY_EVENT_RECORD` |
| Named keys (F1–F12, arrows, Home/End/PgUp/PgDn, Ins/Del) | [~] arrows + ESC only | [x] full set via `plat_keys.c` |
| Modifier bits in `ev->mod` | [ ] | [x] from `dwControlKeyState` |
| UTF-8 input round-trip (BMP + supplementary) | [ ] no multi-byte assembly | [x] surrogate pairs combined before emit; latch cleared on every non-completing return path |
| UTF-8 output round-trip in render path | [ ] render-cell is a stub | [x] `lt__utf8_encode` writes 1–4 bytes |
| `LT_EVENT_RESIZE` delivered exactly once per visible-size change | [ ] no `SIGWINCH` handler | [x] `WINDOW_BUFFER_SIZE_EVENT` filtered for spurious events |
| Diff-based `lt_present` (skip unchanged cells) | n/a (no output) | [x] shared loop skips equal cells; first-frame is a wire no-op (back == front from `lt__buffer_resize`) |
| Cursor-position cache (skip jump on natural advance) | n/a | [x] `(lt__g.cur_x, lt__g.cur_y)` invalidated on present-entry, post-emit-with-wrap-safety, and on resize |
| Alt-screen UX (prior scrollback preserved on exit) | [ ] | [x] `\x1b[?1049h` on init / `\x1b[?1049l` on shutdown |
| Hand-rolled int-to-decimal in render path (no `snprintf`) | n/a | [x] `lt__plat_move_cursor` writes digits directly |
| Bench harness (`examples/bench_present.c`) | n/a | [x] three scenarios (no-change / one-cell / full-repaint) timed via QPC |
| SGR / color emission | [ ] | [ ] |
| Mouse events | [ ] | [ ] |

---

## Known blockers

These are the things that, if fixed, would move the largest number of `[~]` rows above to `[x]`.

1. **POSIX raw-mode entry is missing.** `src/platform/posix/plat_init.c` returns `LT_OK` without touching termios; `lt__plat_get_size` does not call `TIOCGWINSZ`. Until this lands, every POSIX render-path and input-path row stays partial regardless of how complete the shared code is.
2. **POSIX `plat_output.c` is entirely no-ops.** `lt__plat_write` writes to fd 1 directly (no buffering, no flush phase), and `move_cursor` / `clear_screen` / `hide_cursor` / `show_cursor` / `render_cell` do nothing. The shared diff loop in `lt_present` is correct, but it has no sink.
3. **POSIX input lacks an ESC-sequence parser.** Single-byte reads cannot disambiguate F-keys, modified keys, or paste mode. Until the parser exists, `lt_poll_event` on POSIX cannot reach Windows-side parity.
4. **No SGR emission anywhere.** `lt__plat_render_cell` ignores `cell->fg` / `cell->bg` on both platforms. Color, attribute bits, and `lt_set_output_mode` all become live the moment this is wired up.
5. **UTF-8 helpers are private.** `lt__utf8_encode` / `_decode` / `_char_length` are implemented but not re-exported under the public `lt_utf8_*` names termbox2 consumers expect.

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
6. All four output color modes render correctly on Windows Terminal, xterm, tmux, macOS Terminal, and the Linux console.
7. Sanitizer CI is green (Linux gcc + clang, Windows MSVC); fuzzer runs an hour without crashes; valgrind reports no leaks.
8. `compat/termbox2.h` drop-in layer exists and a non-trivial example (small text editor) builds against it.
