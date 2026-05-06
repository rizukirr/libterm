/*
 * src/intrinsics/avx2.c
 *
 * x86_64 AVX2 implementation of the SIMD diff contract.
 * Compiled when LIBTERM_SIMD=avx2 (or auto resolves to it)
 * Compile flag set per-TU in src/CMakeLists.txt
 *
 *   - GCC/Clang: -mavx2
 *   - MSVC:      /arch:AVX2
 */

#include "common.h"
#include "diff.h"
#include "libterm/libterm.h"

#include <immintrin.h>
#include <stdint.h>

/* Cell layout: {u32 ch, u32 fg, u32 bg} = 12 bytes. AVX2 ymm = 32 bytes
 * They don't devide cleanly, but byte-equal across [0, b) implies cell-equal
 * for cells fully in [0, b). So byte-level SIMD scan + (byte_idx / 12) gives
 * a correct first-differing-cell index. */

int lt__simd_diff_first_differ_cell(const struct lt_cell *a,
                                    const struct lt_cell *b, int count) {
  if (count <= 0)
    return count;

  const char *pa = (const char *)a;
  const char *pb = (const char *)b;
  size_t n = (size_t)count * sizeof(struct lt_cell);
  size_t i = 0;

  for (; i + 32 <= n; i += 32) {
    __m256i va = _mm256_loadu_si256((const __m256i *)(pa + i));
    __m256i vb = _mm256_loadu_si256((const __m256i *)(pb + i));
    uint32_t eq = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(va, vb));
    if (eq != 0xFFFFFFFF) {
      size_t byte_idx = i + (size_t)lt__ctz32(~eq);
      return (int)(byte_idx / sizeof(struct lt_cell));
    }
  }

  for (; i < n; i++)
    if (pa[i] != pb[i])
      return (int)(i / sizeof(struct lt_cell));

  return count;
}

/* Note: byte-level "first equal" doesn't translate to cell-level, a single
 * matching byte in a partly-matching cell would be a false positive. For
 * correctnexx we walk cell-by-cell with scalar 3-field equality. The inner
 * phase usage in lt_present is N=run_length, typically small, so scalar is
 * acceptable. Cell-aligned SIMD reduction is a future optimization. */
int lt__simd_diff_first_equal_cell(const struct lt_cell *a,
                                   const struct lt_cell *b, int count) {
  for (int i = 0; i < count; i++) {
    if (a[i].ch != b[i].ch || a[i].fg != b[i].fg || a[i].bg != b[i].bg)
      return i;
  }
  return count;
}
