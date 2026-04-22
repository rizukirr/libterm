# AGENTS.md

Guidance for AI agents working on **libterm** — a cross-platform (Windows + POSIX) terminal UI library derived from [termbox2](https://github.com/termbox/termbox2).

## Project overview

libterm is a C library that provides a termbox2-compatible API on both POSIX and native Windows (Win32 Console API). Unlike termbox2, it is **not header-only** — it ships as a compiled library with a public header plus platform-specific implementation files.

Upstream reference: `external/termbox2/termbox2.h` (read-only, do not modify).

## Naming conventions

All identifiers are renamed from termbox's `tb_` prefix to libterm's `lt_` prefix. This applies uniformly:

| termbox2         | libterm          |
|------------------|------------------|
| `tb_init`        | `lt_init`        |
| `tb_event`       | `lt_event`       |
| `TB_KEY_*`       | `LT_KEY_*`       |
| `TB_OK`, `TB_ERR_*` | `LT_OK`, `LT_ERR_*` |
| internal `tb_*`  | internal `lt_*`  |

Macros, typedefs, enums, structs, functions, and file-local statics all follow the rename. When porting code from `termbox2.h`, rewrite every `tb`/`TB` token — do not leave mixed prefixes.

## Directory layout

```
include/libterm/libterm.h     # public API (lt_* declarations, LT_* macros)
src/
  shared/                     # platform-independent code
    buffer.c                  # back/front buffer, cell diffing
    input.c                   # platform-agnostic event dispatch
    utf8.c                    # UTF-8 encode/decode
    ...
  platform/
    posix/                    # termios, ANSI escapes, select/poll, signals
      init.c
      output.c
      input.c
      ...
    windows/                  # Win32 Console API, ReadConsoleInput, etc.
      init.c
      output.c
      input.c
      ...
external/termbox2/            # upstream reference — read-only
```

**Rule:** platform-specific code lives *only* under `src/platform/<os>/`. Shared code must not `#ifdef _WIN32` — if a difference is needed, expose a small internal function from the platform layer (e.g. `lt__plat_write`, `lt__plat_poll`) and let shared code call it.

## Porting strategy

1. Copy a logical section from `external/termbox2/termbox2.h` (it's one giant file — split it).
2. Decide: shared logic, or platform-specific?
   - Buffer diffing, UTF-8, public API surface → `src/shared/`
   - Terminal I/O, input reading, resize detection, color output → `src/platform/<os>/`
3. Rename all `tb`/`TB` identifiers to `lt`/`LT`.
4. For POSIX, behavior should match termbox2 byte-for-byte where practical.
5. For Windows, **reimplement natively** using the Win32 Console API:
   - `SetConsoleMode` with `ENABLE_VIRTUAL_TERMINAL_PROCESSING` is acceptable for output, but input should prefer `ReadConsoleInputW` for accurate key/mouse/resize events.
   - Use wide-char APIs; convert to/from UTF-8 at the boundary.
   - Do not depend on a POSIX compatibility shim (no cygwin/msys assumptions).

## Public API boundary

The only header consumers include is `include/libterm/libterm.h`. It must:
- Compile as C99 on both platforms.
- Expose only `lt_*` / `LT_*` symbols.
- Contain no platform `#ifdef`s visible to the user (internal header may).
- Keep the termbox2 function signatures where possible, translated to `lt_` names — this gives users a near drop-in migration path.

Internal shared declarations go in `src/shared/internal.h` (not installed).

## Build

Target build system: **CMake** (portable across MSVC, MinGW, clang, gcc).
- Static + shared library targets.
- `examples/` for demos (mirrors `external/termbox2/demo/`).
- `tests/` for unit tests (mirrors `external/termbox2/tests/`).

Do not introduce autotools, Meson, or platform-specific build scripts without discussing first.

## What not to do

- Do not modify anything under `external/termbox2/` — it's the reference.
- Do not put `#ifdef _WIN32` in `src/shared/`.
- Do not keep any `tb_` / `TB_` identifier in libterm source — the rename must be total.
- Do not make this header-only. Implementation lives in `.c` files.
- Do not add third-party dependencies; the whole point is a thin, self-contained library.

## When in doubt

Check how termbox2 does it in `external/termbox2/termbox2.h`, then decide whether that approach ports cleanly to Windows. If it doesn't, write a platform abstraction rather than leaking `#ifdef`s into shared code.
