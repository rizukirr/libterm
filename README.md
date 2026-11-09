# libterm

libterm is a small C11 terminal UI library with a [termbox2](https://github.com/termbox/termbox2)-compatible API. The same code drives POSIX terminals and the native Win32 Console, with no POSIX shims and no `#ifdef` in your app. All identifiers use the `lt_` and `LT_` prefixes.

Draw into an off-screen cell buffer and call `lt_present()`. libterm emits only the cells that changed, inside a synchronized-update bracket, so output is flicker-free by construction. Modern protocols such as the kitty keyboard protocol, 24-bit color and OSC color queries are negotiated automatically and degrade gracefully where missing.

## Features

| Area | What you get |
|---|---|
| API | termbox2's API with `tb_` and `TB_` renamed to `lt_` and `LT_`. Include `compat/termbox2.h` to build existing termbox2 code unchanged ([porting guide](https://github.com/rizukirr/libterm/wiki/Porting-from-termbox2)). [ROADMAP.md](ROADMAP.md) tracks per-symbol status. |
| Platforms | `/dev/tty` with termios raw mode on POSIX, the Win32 Console API on Windows. Shared code has no platform `#ifdef`. |
| Rendering | Double-buffered diff with a SIMD cell scan (AVX2, AVX-512, NEON, SVE, RVV, scalar fallback). All backends for your architecture are compiled in and the best one is picked at runtime, so one binary runs everywhere. Pin one with `-DLIBTERM_SIMD=<backend>`. |
| Unicode | UTF-8, `wcwidth` with wide and zero-width handling, grapheme clusters (combining marks, ZWJ emoji). |
| Color | 8-color, 256, 216-cube, grayscale and 24-bit truecolor output modes. Bold, dim, italic, underline, blink, reverse and strike attributes. |
| Color queries | `lt_query_color` and `lt_is_dark_background`. POSIX does a timeout-bounded OSC 10/11/4 round-trip and keeps input typed meanwhile. Windows answers from the console color table, with palette indexes above 15 returning `LT_ERR_UNSUPPORTED_TERM`. No answer yields `LT_ERR_NO_EVENT`. Under tmux the reply is tmux's own colors. |
| Input | One `lt_event` surface on both platforms: modifiers on every key including Shift+letter, bare-modifier keys, press, repeat and release in `ev.action`, mouse and resize. POSIX negotiates the [kitty keyboard protocol](https://sw.kovidgoyal.net/kitty/keyboard-protocol/) and falls back to legacy decoding (xterm, SS3, rxvt, Linux console, vt220 tilde forms with CSI modifier suffixes). Windows reads the same events from the console API, with best-effort Ctrl+letter disambiguation. `LT_INPUT_COMPAT` restores termbox2 control-byte semantics for drop-in code. |

## Build

Requires CMake 3.32 or newer and a C11 compiler.

```sh
cmake -B build
cmake --build build
ctest --test-dir build      # run the test suite
./build/examples/hello      # try an example
```

### Prebuilt binaries

Every [release](https://github.com/rizukirr/libterm/releases) ships install trees for Linux (x86_64, aarch64, riscv64), Windows (x86_64, MinGW) and macOS (arm64, x86_64), with a `SHA256SUMS` file.

```sh
tar -xzf libterm-v0.1.2-linux-x86_64.tar.gz
sha256sum -c SHA256SUMS --ignore-missing
cmake -B build -DCMAKE_PREFIX_PATH=$PWD/libterm-v0.1.2-linux-x86_64   # then find_package(Libterm)
```

The macOS binaries are unsigned. Run `xattr -d com.apple.quarantine` on the extracted files or build from source.

## Quick start

```c
#include "libterm/libterm.h"

int main(void) {
  if (lt_init() != LT_OK)
    return 1;

  lt_print(2, 1, LT_GREEN, LT_DEFAULT, "hello, libterm, press q to quit");
  lt_present();

  struct lt_event ev;
  while (lt_poll_event(&ev) == LT_OK) {
    if (ev.type == LT_EVENT_KEY && (ev.ch == 'q' || ev.key == LT_KEY_ESC))
      break;
  }

  lt_shutdown();
  return 0;
}
```

## Use it in your project

With CMake, fetch libterm and link the exported target:

```cmake
include(FetchContent)
FetchContent_Declare(libterm
    GIT_REPOSITORY https://github.com/rizukirr/libterm
    GIT_TAG        v0.1.2)
FetchContent_MakeAvailable(libterm)

target_link_libraries(myapp PRIVATE Libterm::libterm)
```

A vendored checkout works with `add_subdirectory(libterm)`. As a subproject, libterm builds only the static library. Examples, benchmarks, tests, the shared library and install rules are opt-in through their `LIBTERM_*` options (`cmake -LH` lists them).

`Libterm::libterm` is the static library. `Libterm::static` and `Libterm::shared` are also exported. The public header is `include/libterm/libterm.h`.

### Without CMake

Installed trees, from a release tarball or `cmake --install`, ship a pkg-config file:

```sh
export PKG_CONFIG_PATH=/path/to/prefix/lib/pkgconfig  # only if not a system prefix
cc app.c $(pkg-config --cflags --libs libterm)
```

libterm is plain C11 with no dependencies, so the scalar build is one compiler invocation (CI runs these lines in `tests/smoke_manual_build.sh`):

```sh
cc -std=c11 -c src/shared/*.c src/platform/posix/*.c src/intrinsics/scalar.c \
    -Iinclude -Isrc
ar rcs libterm.a *.o
```

On Windows, swap `src/platform/posix/*.c` for `src/platform/windows/*.c`. No defines are needed. Manual builds get the scalar backend, runtime-dispatched SIMD needs the CMake build.

## Examples

Built into `build/examples/`:

| Example | Shows |
|---|---|
| `hello` | minimal init, draw and event loop |
| `print` | `lt_print` and `lt_printf`, UTF-8, wide chars, multi-line text |
| `colors` | named colors and attributes |
| `truecolor` | 24-bit RGB, `LT_RGB`, color-depth detection |
| `theme` | `lt_query_color` and `lt_is_dark_background`, with the recommended dark fallback |
| `resize` | layout derived from `lt_width()` and `lt_height()`, redrawn on `LT_EVENT_RESIZE` |
| `keyboard` | termbox2's on-screen keyboard demo, ported by prefix swap plus `LT_INPUT_COMPAT` |
| `kbd` | on-screen keyboard and live event inspector, with an input-model indicator (kitty, legacy, Win32) |
| `mouse` | SGR mouse events |
| `editor` | a small text editor built against `compat/termbox2.h`: open a file, edit, save as, `Ctrl-S` and `Ctrl-Q` |

## Documentation

The [project wiki](https://github.com/rizukirr/libterm/wiki) holds the API reference and guides, including [Porting from termbox2](https://github.com/rizukirr/libterm/wiki/Porting-from-termbox2): the drop-in header, what is supported, and the Ctrl+letter input-model gotcha with both fixes.

## License

MIT
