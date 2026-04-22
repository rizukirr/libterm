#include "../../internal.h"
#include "../../platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* TODO: VK_* → LT_KEY_* mapping table
 *   VK_F1..F12        → LT_KEY_F1..F12
 *   VK_UP/DOWN/...    → LT_KEY_ARROW_*
 *   VK_HOME/END/etc.  → LT_KEY_HOME/END/...
 *   plus Ctrl/Alt/Shift bit translation into lt_event.mod
 */
