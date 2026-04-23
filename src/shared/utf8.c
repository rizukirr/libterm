#include "internal.h"

int lt__utf8_char_length(char c) {
  unsigned char u = (unsigned char)c;
  if ((u & 0x80) == 0x00)
    return 1;
  if ((u & 0xE0) == 0xC0)
    return 2;
  if ((u & 0xF0) == 0xE0)
    return 3;
  if ((u & 0xF8) == 0xF0)
    return 4;
  return 0;
}

int lt__utf8_decode(const char *s, size_t len, lt_uchar *out) {
  (void)s;
  (void)len;
  (void)out;
  /* TODO */
  return 0;
}

int lt__utf8_encode(lt_uchar ch, char out[4]) {
  (void)ch;
  (void)out;
  /* TODO */
  return 0;
}
