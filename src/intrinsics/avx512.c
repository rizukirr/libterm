/*
 * src/intrinsics/avx512.c
 *
 * x86_64 AVX-512 implementation of the SIMD diff contract.
 * Compiled when LIBTERM_SIMD=avx512 (auto does NOT pick this — explicit opt-in).
 * Compile flags set per-TU in src/CMakeLists.txt:
 *   - GCC/Clang: -mavx512f -mavx512bw
 *   - MSVC:      /arch:AVX512
 *
 * AVX-512 has native mask registers, so the byte-equal compare returns a
 * 64-bit mask directly (no movemask trick needed).
 */
#include "intrinsics/common.h"
#include "intrinsics/diff.h"
#include "libterm/libterm.h"

#include <immintrin.h>
#include <stdint.h>

int lt__simd_diff_first_differ_cell(const struct lt_cell *a,
                                    const struct lt_cell *b, int count) {
  if (count <= 0)
    return count;

  const char *pa = (const char *)(const void *)a;
  const char *pb = (const char *)(const void *)b;
  size_t n = (size_t)count * sizeof(struct lt_cell);
  size_t i = 0;

  for (; i + 64 <= n; i += 64) {
    __m512i va = _mm512_loadu_si512((const void *)(pa + i));
    __m512i vb = _mm512_loadu_si512((const void *)(pb + i));
    __mmask64 eq = _mm512_cmpeq_epu8_mask(va, vb);
    if (eq != 0xFFFFFFFFFFFFFFFFull) {
      uint64_t diff = ~(uint64_t)eq;
      size_t byte_idx = i + (size_t)lt__ctz64(diff);
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

/* See avx2.c for reasoning — byte-level "first equal" doesn't translate to
 * cell-level. Inner-phase scan stays scalar. */
int lt__simd_diff_first_equal_cell(const struct lt_cell *a,
                                   const struct lt_cell *b, int count) {
  for (int i = 0; i < count; i++) {
    if (a[i].ch == b[i].ch && a[i].fg == b[i].fg && a[i].bg == b[i].bg)
      return i;
  }
  return count;
}
