#include "libterm/libterm.h"
#include <assert.h>
#include <string.h>

int main(void) {
  assert(strcmp(lt_version(), "0.1.0") == 0);
  assert(lt_strerror(LT_OK) != 0);

  /* not initialized yet */
  assert(lt_present() == LT_ERR_NOT_INIT);
  assert(lt_shutdown() == LT_ERR_NOT_INIT);

  /* lifecycle happy path */
  assert(lt_init() == LT_OK);
  assert(lt_width() > 0);
  assert(lt_height() > 0);
  assert(lt_clear() == LT_OK);
  assert(lt_present() == LT_OK);
  assert(lt_shutdown() == LT_OK);

  /* after shutdown, API should reject render */
  assert(lt_present() == LT_ERR_NOT_INIT);
  return 0;
}
