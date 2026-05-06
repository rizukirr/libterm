/*
 * src/intrinsics/diff.h
 *
 * Cell-level diff and equal scans. Implementations live in
 * scalar.c / avx2.c / neon.c / etc. Exactly one is compiled per build,
 * selected via LIBTERM_SIMD CMake option. Every implementation must
 * produce identical result for the same input
 */

#ifndef LIBTERM_INTRINSICS_DIFF_H
#define LIBTERM_INTRINSICS_DIFF_H

#include "libterm/libterm.h"

/* Returns the index of the first cell in [0, count) where a[i] != b[i].
 * Returns count if all cells equal. count must be >= 0. */
int lt__simd_diff_first_differ_cell(const struct lt_cell *a,
                                    const struct lt_cell *b, int count);

/* Returns the index of the first cell in [0, count) where a[i] == b[i].
 * Returns count if all cells differ. count must be >= 0. */
int lt__simd_diff_first_equal_cell(const struct lt_cell *a,
                                   const struct lt_cell *b, int count);

#endif /* LIBTERM_INTRINSICS_DIFF_H */
