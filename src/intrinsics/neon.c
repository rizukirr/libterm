/*
 * src/intrinsics/neon.c
 *
 * AArch64 NEON implementation of the SIMD diff contract.
 * Compiled when LIBTERM_SIMD=neon (auto picks this on aarch64).
 * No special compile flag needed — NEON is mandatory in the AArch64 baseline.
 *
 * Key wrinkle: NEON has no native movemask. We compress 16 lanes of byte
 * compare results (each 0xFF or 0x00) into a 64-bit integer using the
 * vshrn-by-4 trick: each byte becomes a 4-bit nibble (0xF or 0x0).
 * find-first-differing-byte = ctz(~mask) / 4.
 */
#include "intrinsics/common.h"
#include "intrinsics/diff.h"
#include "libterm/libterm.h"

#include <arm_neon.h>
#include <stdint.h>

/* Compress 16x8 byte vector (each byte 0xFF or 0x00) to 64-bit nibble mask. */
static inline uint64_t lt__neon_movemask(uint8x16_t v) {
  uint8x8_t narrowed = vshrn_n_u16(vreinterpretq_u16_u8(v), 4);
  return vget_lane_u64(vreinterpret_u64_u8(narrowed), 0);
}

int lt__simd_diff_first_differ_cell(const struct lt_cell *a,
                                    const struct lt_cell *b, int count) {
  if (count <= 0)
    return count;

  const char *pa = (const char *)(const void *)a;
  const char *pb = (const char *)(const void *)b;
  size_t n = (size_t)count * sizeof(struct lt_cell);
  size_t i = 0;

  for (; i + 16 <= n; i += 16) {
    uint8x16_t va = vld1q_u8((const uint8_t *)(pa + i));
    uint8x16_t vb = vld1q_u8((const uint8_t *)(pb + i));
    uint8x16_t eq = vceqq_u8(va, vb);
    uint64_t mask = lt__neon_movemask(eq);
    if (mask != 0xFFFFFFFFFFFFFFFFull) {
      /* each byte became a 4-bit nibble; first differing byte at lane = ctz / 4 */
      uint64_t diff = ~mask;
      size_t lane = (size_t)lt__ctz64(diff) >> 2; /* /4 */
      size_t byte_idx = i + lane;
      return (int)(byte_idx / sizeof(struct lt_cell));
    }
  }

  /* scalar tail */
  for (; i < n; i++) {
    if (pa[i] != pb[i])
      return (int)(i / sizeof(struct lt_cell));
  }
  return count;
}

int lt__simd_diff_first_equal_cell(const struct lt_cell *a,
                                   const struct lt_cell *b, int count) {
  for (int i = 0; i < count; i++) {
    if (a[i].ch == b[i].ch && a[i].fg == b[i].fg && a[i].bg == b[i].bg)
      return i;
  }
  return count;
}
