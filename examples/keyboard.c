#include "libterm/libterm.h"
#include <stdio.h>

/* TODO: port from external/termbox2/demo/keyboard.c */
int main(void) {
    if (lt_init() != LT_OK) return 1;
    lt_shutdown();
    return 0;
}
