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
  if (ch < 0x80) {
    out[0] = (char)ch;
    return 1;
  }

  if (ch < 0x800) {
    out[0] = (char)(0xC0 | (ch >> 6));
    out[1] = (char)(0x80 | (ch & 0x3F));
    return 2;
  }

  if (ch < 0x10000) {
    if (ch >= 0xD800 && ch <= 0xDFFF)
      return 0;

    out[0] = (char)(0xE0 | (ch >> 12));
    out[1] = (char)(0x80 | ((ch >> 6) & 0x3F));
    out[2] = (char)(0x80 | (ch & 0x3F));
    return 3;
  }

  if (ch < 0x110000) {
    out[0] = (char)(0xF0 | (ch >> 18));
    out[1] = (char)(0x80 | ((ch >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((ch >> 6) & 0x3F));
    out[3] = (char)(0x80 | (ch & 0x3F));
    return 4;
  }

  return 0;
}
