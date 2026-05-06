/*
 * src/intrinsics/sve.c
 *
 * AArch64 SVE / SVE2 implementation of the SIMD diff contract.
 * Compiled when LIBTERM_SIMD=sve (auto does NOT pick this — explicit opt-in).
 * Compile flag: -march=armv8-a+sve (GCC/Clang).
 *
 * SVE has scalable vector length (128–2048 bits). Predicate registers are
 * first-class: comparisons return predicates, find-first-active is one
 * native instruction (svbrkb_z + svcntp).
 */
#include "intrinsics/common.h"
#include "intrinsics/diff.h"
#include "libterm/libterm.h"

#include <arm_sve.h>
#include <stdint.h>

int lt__simd_diff_first_differ_cell(const struct lt_cell *a,
                                    const struct lt_cell *b, int count) {
  if (count <= 0)
    return count;

  const char *pa = (const char *)(const void *)a;
  const char *pb = (const char *)(const void *)b;
  size_t n = (size_t)count * sizeof(struct lt_cell);
  size_t i = 0;

  uint64_t vlen = svcntb(); /* bytes per SVE vector at runtime */

  while (i + vlen <= n) {
    svbool_t pg = svptrue_b8();
    svuint8_t va = svld1_u8(pg, (const uint8_t *)(pa + i));
    svuint8_t vb = svld1_u8(pg, (const uint8_t *)(pb + i));
    svbool_t neq = svcmpne_u8(pg, va, vb);
    if (svptest_any(pg, neq)) {
      /* break-before-first-true: predicate of all true lanes preceding the
       * first true in `neq`. svcntp counts how many. */
      svbool_t before = svbrkb_z(pg, neq);
      uint64_t lane = svcntp_b8(pg, before);
      size_t byte_idx = i + (size_t)lane;
      return (int)(byte_idx / sizeof(struct lt_cell));
    }
    i += vlen;
  }

  /* scalar tail for any partial trailing region */
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
