# Changelog

All notable changes to libterm are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
adheres to [Semantic Versioning](https://semver.org/) (pre-1.0: minor versions
may contain breaking API changes; patch versions never do).

## [Unreleased]

### Fixed
- **POSIX input:** the continuation bytes of a UTF-8 character were awaited only
  for the escape-sequence window (50 ms). Over ssh (Windows client → Linux host)
  the bytes of one keystroke can arrive further apart; the lead byte then became
  `U+FFFD` and the tail a stray byte. A pending lead byte can never be a complete
  key by itself, so the tail reader now waits up to `LT__UTF8_TAIL_TIMEOUT_MS`
  (500 ms); the ESC disambiguation window is unchanged. Test: split `ä` with a
  200 ms gap in `tests/test_posix_input_frag.c`.

## [0.1.2] - 2026-06-15

Additive API release. The core API and ABI are unchanged from 0.1.1 — every
existing `lt_*` symbol behaves identically; `lt_version()` now returns
`"0.1.2"`. No code changes are required in callers.

### Added
- **`LT_HEX(rgb)`** — pack a `0xRRGGBB` hex color into an `lt_attr`, complementing
  `LT_RGB(r,g,b)`. High bits (attribute flags + the `LT_HI_BLACK` sentinel) are
  masked, so it is a pure color packer; `LT_HEX(0xRRGGBB) == LT_RGB(0xRR,0xGG,0xBB)`.

## [0.1.1] - 2026-06-13

Tooling + compatibility release. The core library API and ABI are unchanged
from 0.1.0 (every `lt_*` symbol behaves identically); `lt_version()` now
returns `"0.1.1"`. No code changes were required in callers.

### Added
- **`compat/termbox2.h`** — a single drop-in header mapping the supported
  termbox2 `tb_`/`TB_` API onto libterm, so existing termbox2 programs compile
  and link unchanged. Functions and the full `TB_KEY_*` / `TB_MOD_*` /
  `TB_EVENT_*` / `TB_OUTPUT_*` / `TB_INPUT_*` constant set, colors, attributes,
  return codes, and `struct tb_cell` / `struct tb_event` / `uintattr_t` are
  aliased; `tb_get_cell` / `tb_put_cell` are adapted; the five functions libterm
  has no equivalent for (`tb_init_rwfd`, `tb_set_func`, `tb_has_truecolor`,
  `tb_cell_buffer`, `tb_key_i`) become a compile error naming the `lt_`
  alternative. `tb_init` keeps libterm's modern key model; opt into termbox2
  control-byte semantics with `tb_set_input_mode(LT_INPUT_COMPAT)`.
- **`examples/editor.c`** — a minimal text editor (open/edit/save, cursor
  movement, a Save As prompt, `Ctrl-S` / `Ctrl-Q`) built only against
  `compat/termbox2.h`.
- **Fuzzing** — libFuzzer harnesses for the three untrusted-input parsers (the
  escape-sequence decoder, the OSC color-reply parser, and the UTF-8 decoder),
  with seed corpora, a per-PR smoke run, and a nightly ~1h-per-target run
  (`-DLIBTERM_BUILD_FUZZERS=ON`, clang-only).
- **valgrind** — a memcheck CI job over the pure-unit test subset (new
  `valgrind_safe` CTest label), catching uninitialized-memory reads the
  AddressSanitizer/UBSan job does not; fails on definite and indirect leaks.

### Documentation
- New wiki page **Porting from termbox2** (the compat header, what's supported,
  and the `Ctrl`+letter input-model gotcha with both fixes); a fuzzing how-to
  (`tests/fuzz/README.md`); README pointers to the compat header, the porting
  guide, and the editor example.

## [0.1.0] - 2026-06-06

First release. (Re-tagged 2026-06-06 to fold in the packaging and
consumption work that landed immediately after the original 2026-06-05 tag,
and again 2026-06-07 to fold in the input-correctness fixes below —
key-release suppression, Shift+letter text — found by the first real
consumer.)

### Changed
- Key-release events are no longer delivered by default. On terminals that
  report them (kitty keyboard protocol on POSIX, the Win32 console), a caller
  that switched on `key`/`ch` without checking `ev.action` handled every
  keystroke twice — typing showed doubled characters. Releases are now dropped
  at delivery; OR the new `LT_INPUT_RELEASE` flag into `lt_set_input_mode` to
  receive them. Press and repeat events are unaffected.

### Fixed
- Shift+letter on kitty-capable terminals now reports the shifted character
  (`ch='J'`, `LT_MOD_SHIFT`) instead of the base codepoint (`'j'`). The kitty
  negotiation requests the report-associated-text flag (`CSI > 27 u`) and the
  CSI-u decoder prefers the layout-translated text codepoint for `ch` — parity
  with the Windows console and legacy POSIX paths. Caps Lock typing is covered
  by the same mechanism.
- `lt_arena_restore` freed blocks with raw `free()` instead of the platform
  allocation macro, mismatching `HeapAlloc` on Windows (latent — no caller
  yet).

### Added
- Prebuilt release binaries: every published GitHub release automatically
  gets install-tree archives for 6 targets (linux x86_64/aarch64/riscv64,
  windows x86_64 MinGW, macos arm64/x86_64) plus a `SHA256SUMS` file, built in
  Release mode with runtime-dispatched SIMD and gated by a tag-vs-CMake-version
  guard (`.github/workflows/release.yml`).
- Consumable as a subproject: `FetchContent` / `add_subdirectory` builds get
  exactly one target (the static library) — `LIBTERM_BUILD_SHARED`,
  `LIBTERM_BUILD_EXAMPLES`, `LIBTERM_BUILD_BENCH`, `LIBTERM_BUILD_TESTS`, and
  `LIBTERM_INSTALL` default to ON only when libterm is the top-level project,
  and everything stays opt-in. Guarded by the CI `fetchcontent-smoke` job
  (ubuntu / windows / macos): build+link, no leaked targets, an
  uninstrumented library (also with `LIBTERM_BUILD_BENCH=ON`), no install
  pollution (`tests/smoke_fetchcontent.sh`).
- pkg-config support: installed trees ship `lib/pkgconfig/libterm.pc`,
  relocatable (`${pcfiledir}`-relative prefix) so it works wherever a release
  tarball is extracted. CI-guarded in `tests/smoke_install.sh`, including a
  moved-prefix pass. Artifacts follow Unix naming convention — `libterm.a` /
  `libterm.so` / `libterm.dll`, link flag `-lterm`. `find_package(Libterm)`
  version compatibility is `SameMinorVersion` while pre-1.0 (switches to
  `SameMajorVersion` automatically at 1.0).
- "Without CMake" README section: pkg-config consumption plus the manual
  `cc`+`ar` scalar build, the latter CI-executed verbatim by
  `tests/smoke_manual_build.sh` so the documentation cannot rot.
- termbox2 cursor parity: the terminal cursor is hidden by default after
  `lt_init`; `lt_set_cursor` shows it (and `lt_present` keeps it parked at
  the set position across frames), `lt_hide_cursor` hides it again.
- Resize correctness end-to-end: a size change forces the next `lt_present`
  to repaint every cell (no stale terminal content after shrinks); both
  platform resize paths share `lt__handle_resize`, with exactly-once
  delivery, burst coalescing, and no-op suppression validated by tests.
- Bench instrumentation is isolated: benches link a dedicated
  `libterm_bench` copy carrying `LIBTERM_ENABLE_RENDER_STATS`; the shipped
  `libterm_static` / `libterm_shared` are always clean.
- termbox2-compatible API (`lt_`/`LT_` prefix) on POSIX and native Win32
  Console; three documented intentional divergences.
- Double-buffered diff renderer with SIMD cell scans (AVX2/AVX-512/NEON/SVE/
  RVV, scalar fallback) inside synchronized-update brackets.
- Modern input model: kitty keyboard protocol negotiation with legacy
  fallback; press/repeat/release actions; bare-modifier keys; SGR mouse;
  `LT_INPUT_COMPAT` for termbox2 semantics. Stray OSC replies are consumed
  by the decoder, never shredded into key events.
- Output modes: normal/256/216/grayscale/truecolor; UTF-8, wcwidth, grapheme
  clusters.
- Color querying: `lt_query_color` (default fg/bg + palette 0..255, packed
  `0x00RRGGBB`) and `lt_is_dark_background` (BT.709 light/dark verdict).
  POSIX: real OSC 10/11/4 round-trip with timeout; `rgb:` and URxvt `rgba:`
  reply forms; input typed during the query window is preserved. Windows:
  native `GetConsoleScreenBufferInfoEx` color table (palette >15 returns
  `LT_ERR_UNSUPPORTED_TERM`).
- `examples/theme.c` — light/dark-themed panel demonstrating the query API and
  the assume-dark fallback; quits on `q`.
- Install/export packaging: `install(TARGETS)` + `LibtermTargets.cmake`, so
  `find_package(Libterm)` yields working `Libterm::libterm` / `Libterm::static`
  / `Libterm::shared` targets; smoke-tested by `tests/smoke_install.sh` in
  default and shared-only configurations.
- Runtime SIMD dispatch: `LIBTERM_SIMD=auto` now compiles every backend the
  target architecture supports and selects at startup from CPU capabilities
  (x86_64: scalar→AVX2→AVX-512 via cpuid; aarch64: NEON→SVE via hwcap;
  riscv64: scalar→RVV via hwcap) — one binary, fast on capable CPUs, no
  illegal-instruction risk on older ones. Explicit `LIBTERM_SIMD=<backend>`
  still builds a single static backend.
- CI: Linux (4 compiler/SIMD configs), macOS (portable subset + pty lifecycle
  canary; full pty-test parity tracked in ROADMAP Known blockers), native
  Windows (`windows-mingw-native`: MinGW build + ctest), MinGW cross-build,
  install smoke, sanitizers, cross-arch emulation (aarch64 NEON/SVE,
  riscv64 RVV).
