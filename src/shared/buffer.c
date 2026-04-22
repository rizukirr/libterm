#include "internal.h"
#include <stdlib.h>

int lt__buffer_resize(int w, int h) {
    (void)w; (void)h;
    /* TODO: allocate/reallocate lt__g.back and lt__g.front */
    return LT_OK;
}

void lt__buffer_free(void) {
    free(lt__g.back);  lt__g.back  = NULL;
    free(lt__g.front); lt__g.front = NULL;
}

void lt__buffer_clear(struct lt_cell *buf, int count, lt_attr fg, lt_attr bg) {
    for (int i = 0; i < count; i++) {
        buf[i].ch = ' ';
        buf[i].fg = fg;
        buf[i].bg = bg;
    }
}
