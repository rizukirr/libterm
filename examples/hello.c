#include "libterm/libterm.h"
#include <stdio.h>

int main(void) {
    if (lt_init() != LT_OK) {
        fprintf(stderr, "lt_init failed\n");
        return 1;
    }
    lt_clear();
    const char *msg = "Hello from libterm! (press any key)";
    int x = 0;
    for (const char *p = msg; *p; ++p, ++x) {
        lt_set_cell(x, 0, (lt_uchar)*p, LT_WHITE, LT_DEFAULT);
    }
    lt_present();

    struct lt_event ev;
    lt_poll_event(&ev);

    lt_shutdown();
    return 0;
}
