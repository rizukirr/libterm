#include "libterm/libterm.h"
#include <windows.h>

int main(void) {
  if (lt_init() != LT_OK)
    return 1;

  lt_hide_cursor();
  const char *msg = "cursor toggle test";
  for (int i = 0; msg[i]; i++)
    lt_set_cell(2 + i, 1, (lt_uchar)msg[i], 0, 0);
  lt_present();

  Sleep(2000); /* cursor hidden, msg visible */

  lt_set_cursor(2, 3);
  lt_show_cursor();
  Sleep(2000); /* cursor now visible at (2,3) */

  lt_shutdown();
  return 0;
}
