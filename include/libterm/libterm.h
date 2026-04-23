/*
 * libterm - cross-platform terminal UI library
 *
 * Public API. All identifiers use the lt_ / LT_ prefix.
 * Derived from termbox2 (https://github.com/termbox/termbox2).
 */
#ifndef LIBTERM_H
#define LIBTERM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(LIBTERM_SHARED)
#if defined(LIBTERM_BUILDING)
#define LT_API __declspec(dllexport)
#else
#define LT_API __declspec(dllimport)
#endif
#else
#define LT_API
#endif

/* ---- version ---- */
#define LT_VERSION_MAJOR 0
#define LT_VERSION_MINOR 1
#define LT_VERSION_PATCH 0

/* ---- return codes ---- */
#define LT_OK 0
#define LT_ERR -1
#define LT_ERR_NEED_MORE -2
#define LT_ERR_INIT_ALREADY -3
#define LT_ERR_INIT_OPEN -4
#define LT_ERR_MEM -5
#define LT_ERR_NO_EVENT -6
#define LT_ERR_NO_TERM -7
#define LT_ERR_NOT_INIT -8
#define LT_ERR_OUT_OF_BOUNDS -9
#define LT_ERR_READ -10
#define LT_ERR_RESIZE_IOCTL -11
#define LT_ERR_RESIZE_PIPE -12
#define LT_ERR_RESIZE_SIGACTION -13
#define LT_ERR_POLL -14
#define LT_ERR_TCGETATTR -15
#define LT_ERR_TCSETATTR -16
#define LT_ERR_UNSUPPORTED_TERM -17
#define LT_ERR_RESIZE_WRITE -18
#define LT_ERR_RESIZE_POLL -19
#define LT_ERR_RESIZE_READ -20
#define LT_ERR_RESIZE_SSCANF -21
#define LT_ERR_CAP_COLLISION -22

/* ---- event types ---- */
#define LT_EVENT_KEY 1
#define LT_EVENT_RESIZE 2
#define LT_EVENT_MOUSE 3

/* ---- keys (subset; mirror termbox2) ---- */
#define LT_KEY_F1 (0xFFFF - 0)
#define LT_KEY_F2 (0xFFFF - 1)
#define LT_KEY_F3 (0xFFFF - 2)
#define LT_KEY_F4 (0xFFFF - 3)
#define LT_KEY_F5 (0xFFFF - 4)
#define LT_KEY_F6 (0xFFFF - 5)
#define LT_KEY_F7 (0xFFFF - 6)
#define LT_KEY_F8 (0xFFFF - 7)
#define LT_KEY_F9 (0xFFFF - 8)
#define LT_KEY_F10 (0xFFFF - 9)
#define LT_KEY_F11 (0xFFFF - 10)
#define LT_KEY_F12 (0xFFFF - 11)
#define LT_KEY_INSERT (0xFFFF - 12)
#define LT_KEY_DELETE (0xFFFF - 13)
#define LT_KEY_HOME (0xFFFF - 14)
#define LT_KEY_END (0xFFFF - 15)
#define LT_KEY_PGUP (0xFFFF - 16)
#define LT_KEY_PGDN (0xFFFF - 17)
#define LT_KEY_ARROW_UP (0xFFFF - 18)
#define LT_KEY_ARROW_DOWN (0xFFFF - 19)
#define LT_KEY_ARROW_LEFT (0xFFFF - 20)
#define LT_KEY_ARROW_RIGHT (0xFFFF - 21)

#define LT_KEY_ENTER 0x0D
#define LT_KEY_ESC 0x1B
#define LT_KEY_SPACE 0x20
#define LT_KEY_BACKSPACE 0x08
#define LT_KEY_BACKSPACE2 0x7F
#define LT_KEY_TAB 0x09

/* ---- modifiers ---- */
#define LT_MOD_ALT 0x01
#define LT_MOD_CTRL 0x02
#define LT_MOD_SHIFT 0x04
#define LT_MOD_MOTION 0x08

/* ---- colors ---- */
#define LT_DEFAULT 0x0000
#define LT_BLACK 0x0001
#define LT_RED 0x0002
#define LT_GREEN 0x0003
#define LT_YELLOW 0x0004
#define LT_BLUE 0x0005
#define LT_MAGENTA 0x0006
#define LT_CYAN 0x0007
#define LT_WHITE 0x0008

/* ---- attributes ---- */
#define LT_BOLD 0x0100
#define LT_UNDERLINE 0x0200
#define LT_REVERSE 0x0400
#define LT_ITALIC 0x0800
#define LT_BLINK 0x1000
#define LT_DIM 0x2000
#define LT_STRIKE 0x4000

/* ---- types ---- */
typedef uint32_t lt_uchar;
typedef uint32_t lt_attr;

struct lt_cell {
  lt_uchar ch;
  lt_attr fg;
  lt_attr bg;
};

struct lt_event {
  uint8_t type;
  uint8_t mod;
  uint16_t key;
  lt_uchar ch;
  int32_t w;
  int32_t h;
  int32_t x;
  int32_t y;
};

/* ---- lifecycle ---- */
LT_API int lt_init(void);
LT_API int lt_shutdown(void);

/* ---- screen ---- */
LT_API int lt_width(void);
LT_API int lt_height(void);
LT_API int lt_clear(void);
LT_API int lt_set_clear_attrs(lt_attr fg, lt_attr bg);
LT_API int lt_present(void);
LT_API int lt_set_cursor(int x, int y);
LT_API int lt_hide_cursor(void);
LT_API int lt_set_cell(int x, int y, lt_uchar ch, lt_attr fg, lt_attr bg);

/* ---- input ---- */
LT_API int lt_poll_event(struct lt_event *event);
LT_API int lt_peek_event(struct lt_event *event, int timeout_ms);

/* ---- input mode flags ---- */
#define LT_INPUT_CURRENT 0
#define LT_INPUT_ESC 1
#define LT_INPUT_ALT 2
#define LT_INPUT_MOUSE 4

LT_API int lt_set_input_mode(int mode);

/* ---- output mode flags ---- */
#define LT_OUTPUT_CURRENT 0
#define LT_OUTPUT_NORMAL 1
#define LT_OUTPUT_256 2
#define LT_OUTPUT_216 3
#define LT_OUTPUT_GRAYSCALE 4
#define LT_OUTPUT_TRUECOLOR 5

LT_API int lt_set_output_mode(int mode);

/* ---- misc ---- */
LT_API const char *lt_strerror(int code);
LT_API const char *lt_version(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBTERM_H */
