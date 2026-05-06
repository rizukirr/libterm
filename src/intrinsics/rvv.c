/*
 * src/intrinsics/rvv.c
 *
 * RISC-V Vector (RVV 1.0) implementation of the SIMD diff contract.
 * Compiled when LIBTERM_SIMD=rvv (auto does NOT pick this — explicit opt-in).
 * Compile flag: -march=rv64gcv (GCC/Clang).
 *
 * RVV has scalable vector length set per loop via vsetvl. Comparisons
 * return mask registers; vfirst gives the index of the first set bit
 * directly.
 */
#include "intrinsics/common.h"
#include "intrinsics/diff.h"
#include "libterm/libterm.h"

#include <riscv_vector.h>
#include <stdint.h>

int lt__simd_diff_first_differ_cell(const struct lt_cell *a,
                                    const struct lt_cell *b, int count) {
  if (count <= 0)
    return count;

  const char *pa = (const char *)(const void *)a;
  const char *pb = (const char *)(const void *)b;
  size_t n = (size_t)count * sizeof(struct lt_cell);
  size_t i = 0;

  while (i < n) {
    size_t avl = n - i;
    size_t vl = __riscv_vsetvl_e8m1(avl);

    vuint8m1_t va = __riscv_vle8_v_u8m1((const uint8_t *)(pa + i), vl);
    vuint8m1_t vb = __riscv_vle8_v_u8m1((const uint8_t *)(pb + i), vl);
    vbool8_t neq = __riscv_vmsne_vv_u8m1_b8(va, vb, vl);

    /* vfirst returns the index of the first true lane, or -1 if none. */
    long first = __riscv_vfirst_m_b8(neq, vl);
    if (first >= 0) {
      size_t byte_idx = i + (size_t)first;
      return (int)(byte_idx / sizeof(struct lt_cell));
    }

    i += vl;
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
