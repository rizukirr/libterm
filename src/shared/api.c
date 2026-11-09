#include "internal.h"
#include "libterm/libterm.h"
#include "platform.h"

struct lt__state lt__g = {0};

static int lt__finish_init(void) {
  int w = 0, h = 0;

  int rc = lt__plat_get_size(&w, &h);
  if (rc != LT_OK) {
    lt__plat_shutdown();
    return rc;
  }

  rc = lt__buffer_resize(w, h);
  if (rc != LT_OK) {
    lt__plat_shutdown();
    return rc;
  }

  lt__g.cur_x = -1;
  lt__g.cur_y = -1;
  lt__g.cursor_x = -1; /* hidden by default (termbox2 parity) */
  lt__g.cursor_y = -1;
  lt__g.cur_fg = 0xFFFFFFFF;
  lt__g.cur_bg = 0xFFFFFFFF;
  lt__g.cur_attrs = 0xFFFFFFFF;
  /* termbox2 default: lone ESC is its own key, Alt-combos are reported as a
   * separate ESC followed by the key (see lt_set_input_mode / LT_INPUT_ALT). */
  lt__g.input_mode = 0; /* modern model by default (compat is opt-in) */
  /* Modern model: negotiate the kitty keyboard protocol (best-effort). */
  if (!(lt__g.input_mode & LT_INPUT_COMPAT)) {
    if (lt__plat_kitty_enable() == LT_OK)
      lt__g.kitty_active = true;
  }
  lt__g.initialized = 1;
  return LT_OK;
}

int lt_init_fd(int ttyfd) {
  if (lt__g.initialized)
    return LT_ERR_INIT_ALREADY;
  int rc = lt__plat_init_fd(ttyfd, 0);
  if (rc != LT_OK)
    return rc;
  return lt__finish_init();
}

int lt_init(void) {
  if (lt__g.initialized)
    return LT_ERR_INIT_ALREADY;
  int rc = lt__plat_init();
  if (rc != LT_OK)
    return rc;
  return lt__finish_init();
}

int lt_init_file(const char *path) {
  if (lt__g.initialized)
    return LT_ERR_INIT_ALREADY;
  if (!path)
    return LT_ERR;
  int rc = lt__plat_init_file(path);
  if (rc != LT_OK)
    return rc;
  return lt__finish_init();
}

int lt_shutdown(void) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  lt__buffer_free();
  lt__egc_reset();

  if (lt__g.kitty_active) {
    (void)lt__plat_kitty_disable();
    lt__g.kitty_active = false;
  }

  static const char sgr_reset[] = "\x1b[0m";
  (void)lt__plat_write(sgr_reset, sizeof(sgr_reset) - 1);

  int rc = lt__plat_shutdown();
  lt__g.cur_fg = 0xFFFFFFFF;
  lt__g.cur_bg = 0xFFFFFFFF;
  lt__g.cur_attrs = 0xFFFFFFFF;
  lt__g.initialized = 0;
  return rc;
}

int lt_width(void) { return lt__g.width; }
int lt_height(void) { return lt__g.height; }

const char *lt_version(void) { return "0.1.3"; }
