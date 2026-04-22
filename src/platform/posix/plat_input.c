#include "../../internal.h"
#include "../../platform.h"

/* TODO: read from tty fd, parse escape sequences → lt_event */
int lt__plat_read_event(struct lt_event *ev, int timeout_ms) {
    (void)ev; (void)timeout_ms;
    return LT_ERR_NO_EVENT;
}
