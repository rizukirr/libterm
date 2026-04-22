#include "libterm/libterm.h"

/* TODO: mouse + color demo */
int main(void) {
    if (lt_init() != LT_OK) return 1;
    lt_shutdown();
    return 0;
}
