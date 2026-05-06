/*
 * src/intrinsics/common.h
 *
 * Cross-compiler shims for SIMS TUs. The only place compiler-specific
 * code lives, every per-ISA TU includes this and stays clean.
 */

#ifndef LIBTERM_INTRINSICS_COMMON_H
#define LIBTERM_INTRINSICS_COMMON_H

#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
#define LT__INLINE static inline __attribute__((always_inline))
static inline int lt__ctz32(uint32_t x) { return __builtin_ctz(x); }
static inline int lt__ctz64(uint64_t x) { return __builtin_ctzll(x); }
#elif defined(_MSC_VER)
#include <intrin.h>
#define LT__INLINE static __forceinline
static __forceinline int lt__ctz32(uint32_t x) {
  unsigned long i;
  _BitScanForward(&i, x);
  return (int)i;
}
static __forceinline int lt__ctz64(uint64_t x) {
  unsigned long i;
#if defined(_M_X64) || defined(_M_ARM64)
  _BitScanForward64(&i, x);
#else
  if (_BitScanForward(&i, (uint32_t)x))
    return (int)i;
  _BitScanForward(&i, (uint32_t)(x >> 32));
  return (int)i + 32;
#endif
  return (int)i;
}
#else
#define LT__INLINE static inline
static inline int lt__ctz32(uint32_t x) {
  int n = 0;
  while (!(x & 1u)) {
    x >>= 1;
    ++n;
  }
  return n;
}
static inline int lt__ctz64(uint64_t x) {
  int n = 0;
  while (!(x & 1ull)) {
    x >>= 1;
    ++n;
  }
  return n;
}
#endif // compiler

#endif /* LIBTERM_INTRINSICS_COMMON_H */
