#include "libterm/libterm.h"
#include "internal.h"
#include "platform.h"

struct lt__state lt__g = {0};

int lt_init(void) {
    if (lt__g.initialized) return LT_ERR_INIT_ALREADY;
    int rc = lt__plat_init();
    if (rc != LT_OK) return rc;
    lt__g.initialized = 1;
    return LT_OK;
}

int lt_shutdown(void) {
    if (!lt__g.initialized) return LT_ERR_NOT_INIT;
    lt__buffer_free();
    int rc = lt__plat_shutdown();
    lt__g.initialized = 0;
    return rc;
}

int lt_width(void)  { return lt__g.width; }
int lt_height(void) { return lt__g.height; }

const char *lt_version(void) { return "0.1.0"; }
