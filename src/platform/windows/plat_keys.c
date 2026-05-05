#include "libterm/libterm.h"
#include "win_internal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

WORD lt__win_vk_to_lt_key(WORD vk) {
  // clang-format off
  switch(vk){
  /* control */
  case VK_RETURN: return LT_KEY_ENTER;
  case VK_ESCAPE: return LT_KEY_ESC;
  case VK_BACK:   return LT_KEY_BACKSPACE;
  case VK_TAB:    return LT_KEY_TAB;

  /* arrows */
  case VK_UP:     return LT_KEY_ARROW_UP;
  case VK_DOWN:   return LT_KEY_ARROW_DOWN;
  case VK_LEFT:   return LT_KEY_ARROW_LEFT;
  case VK_RIGHT:  return LT_KEY_ARROW_RIGHT;

  /* navigation */
  case VK_INSERT: return LT_KEY_INSERT;
  case VK_DELETE: return LT_KEY_DELETE;
  case VK_HOME:   return LT_KEY_HOME;
  case VK_END:    return LT_KEY_END;
  case VK_PRIOR:  return LT_KEY_PGUP;
  case VK_NEXT:   return LT_KEY_PGDN;

  /* function keys F1..F12 */
  case VK_F1:  return LT_KEY_F1;
  case VK_F2:  return LT_KEY_F2;
  case VK_F3:  return LT_KEY_F3;
  case VK_F4:  return LT_KEY_F4;
  case VK_F5:  return LT_KEY_F5;
  case VK_F6:  return LT_KEY_F6;
  case VK_F7:  return LT_KEY_F7;
  case VK_F8:  return LT_KEY_F8;
  case VK_F9:  return LT_KEY_F9;
  case VK_F10: return LT_KEY_F10;
  case VK_F11: return LT_KEY_F11;
  case VK_F12: return LT_KEY_F12;

  default: return 0;
  }
  // clang-format on
}
