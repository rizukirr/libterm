# AGENTS.md

## What this project is

**libterm** is a rewrite of [termbox2](https://github.com/termbox/termbox2) — same public API, same event semantics, same drawing model, but reorganized into a compiled multi-platform library that runs natively on POSIX (Linux, macOS, BSD) and on Windows (Win32 Console API, no POSIX shim).

The goal is straightforward: take termbox2 and make it better — faster render path, cleaner internals, real Unicode handling, proper Windows support — **without changing how the API behaves**. Anything you can do with `tb_*` in termbox2, you do with `lt_*` in libterm, and you get the same result.

## What "the same behavior" means

- Every public function from termbox2 has a `lt_`-prefixed counterpart with the same signature, same return codes, and same observable effect on the terminal.
- Every `TB_*` macro becomes `LT_*` with the same numeric value where it's externally visible.
- An `lt_event` for a given key, resize, or mouse input matches the `tb_event` termbox2 would have produced for the same input on the same platform.
- A program written against termbox2 should be portable to libterm by renaming `tb` → `lt` and `TB` → `LT`. A `compat/termbox2.h` aliasing header is planned to make even that step unnecessary.

## What "better" means

- **Multi-platform first-class.** termbox2 is POSIX-only; libterm runs natively on Windows using `ReadConsoleInputW`, wide-char console APIs, and VT output, with no POSIX emulation layer.
- **Compiled library, not header-only.** Static and shared targets, proper symbol visibility, single public header.
- **Faster render path.** The diff-based `lt_present` follows refterm's playbook for the parts that matter to a terminal-emitting library: a single reusable output buffer, run-length cell coalescing, cached SGR fragments, and one syscall per frame.
- **Real Unicode.** Grapheme-cluster segmentation (UAX #29 essentials) instead of `wcwidth`-only width estimation, so ZWJ emoji and regional indicators occupy the right number of cells.
- **Cleaner internals.** Strict layering (public API / shared core / platform layer), arena-backed allocation, no `#ifdef _WIN32` leaking into shared code.

See `ROADMAP.md` for the full per-API status matrix on both platforms.

## Naming rule

Every `tb_` / `TB_` token from termbox2 is renamed to `lt_` / `LT_` in libterm — macros, typedefs, enums, structs, functions, and file-local statics. When porting, rewrite every token; mixed prefixes are not allowed.

## Directory layout

```
include/libterm/libterm.h     # public API (lt_* declarations, LT_* macros)
src/
  internal.h                  # shared internal state and helpers
  platform.h                  # contract between shared core and platform layer
  shared/                     # platform-independent code — no #ifdef _WIN32
    api.c                     # lifecycle dispatch
    buffer.c                  # back/front buffer, cell ops
    cell.c                    # lt_set_cell
    input.c                   # event dispatch
    output.c                  # lt_clear / lt_present / cursor / output mode
    utf8.c                    # UTF-8 encode/decode
  platform/
    posix/                    # termios + ANSI + select/poll + signals
    windows/                  # Win32 Console API: ReadConsoleInputW, WriteFile, VT SGR
  lib/
    arena.h                   # arena allocator (no raw malloc/free elsewhere)
external/                     # read-only references (termbox2, refterm)
```

**Layering rule:** platform-specific code lives only under `src/platform/<os>/`. Shared code expresses platform differences by calling small `lt__plat_*` hooks declared in `src/platform.h`. No `#ifdef _WIN32` in `src/shared/`.

## Build and test

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

CMake options: `LIBTERM_BUILD_SHARED`, `LIBTERM_BUILD_STATIC`, `LIBTERM_BUILD_EXAMPLES`, `LIBTERM_BUILD_TESTS`, `LIBTERM_WARNINGS_AS_ERRORS`. Tests link `libterm_static` by default.

## Code style

- LLVM formatting: `clang-format -style=LLVM -i <files>`. Never format anything under `external/`.
- Allocator: use `src/lib/arena.h`. Do not call `malloc` / `calloc` / `realloc` / `free` directly outside the allocator implementation.
- C11 on both platforms.

## Hard rules

- Do not modify anything under `external/`.
- No `#ifdef _WIN32` in `src/shared/` — push the difference into the platform layer.
- No third-party dependencies.
- Do not make the library header-only.
- Do not add another build system without discussing first.
- Do not keep any `tb_` / `TB_` identifier in libterm source — the rename must be total.
