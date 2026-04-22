#include "internal.h"
#include "platform.h"

int lt_clear(void) {
    if (!lt__g.initialized) return LT_ERR_NOT_INIT;
    /* TODO: clear back buffer using clear_fg/clear_bg */
    return LT_OK;
}

int lt_set_clear_attrs(lt_attr fg, lt_attr bg) {
    lt__g.clear_fg = fg;
    lt__g.clear_bg = bg;
    return LT_OK;
}

int lt_present(void) {
    if (!lt__g.initialized) return LT_ERR_NOT_INIT;
    /* TODO: diff back vs front, emit minimal updates via lt__plat_render_cell */
    return lt__plat_flush();
}

int lt_set_cursor(int x, int y) {
    if (!lt__g.initialized) return LT_ERR_NOT_INIT;
    return lt__plat_move_cursor(x, y);
}

int lt_hide_cursor(void) {
    if (!lt__g.initialized) return LT_ERR_NOT_INIT;
    return lt__plat_hide_cursor();
}

int lt_set_output_mode(int mode) {
    if (mode == LT_OUTPUT_CURRENT) return lt__g.output_mode;
    lt__g.output_mode = mode;
    return mode;
}
