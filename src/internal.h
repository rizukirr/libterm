/*
 * libterm - internal shared declarations.
 * Not installed. Do not expose lt__ (double-underscore) symbols publicly.
 */
#ifndef LIBTERM_INTERNAL_H
#define LIBTERM_INTERNAL_H

#include "libterm/libterm.h"
#include <stdbool.h>

/* ---- global state (single terminal instance) ---- */
struct lt__state {
    bool          initialized;
    int           width;
    int           height;
    int           input_mode;
    int           output_mode;
    lt_attr       clear_fg;
    lt_attr       clear_bg;
    struct lt_cell *back;   /* back buffer  (w*h cells) */
    struct lt_cell *front;  /* front buffer (w*h cells) */
};

extern struct lt__state lt__g;

/* ---- buffer ops (shared/buffer.c) ---- */
int  lt__buffer_resize(int w, int h);
void lt__buffer_free(void);
void lt__buffer_clear(struct lt_cell *buf, int count, lt_attr fg, lt_attr bg);

/* ---- utf8 (shared/utf8.c) ---- */
int  lt__utf8_char_length(char c);
int  lt__utf8_decode(const char *s, size_t len, lt_uchar *out);
int  lt__utf8_encode(lt_uchar ch, char out[4]);

#endif /* LIBTERM_INTERNAL_H */
