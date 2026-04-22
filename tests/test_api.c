#include "libterm/libterm.h"
#include <assert.h>
#include <string.h>

int main(void) {
    assert(strcmp(lt_version(), "0.1.0") == 0);
    assert(lt_strerror(LT_OK) != 0);
    return 0;
}
