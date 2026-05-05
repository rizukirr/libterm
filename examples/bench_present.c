#include "libterm/libterm.h"
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define BENCH_ITERS 1000

static double qpc_freq_inv_us = 0.0;
static volatile int lt__bench_sink = 0;

static void qpc_init(void) {
  LARGE_INTEGER f;
  QueryPerformanceFrequency(&f);
  qpc_freq_inv_us = 1000000.0 / (double)f.QuadPart;
}

static long long qpc_now(void) {
  LARGE_INTEGER c;
  QueryPerformanceCounter(&c);
  return c.QuadPart;
}

static double qpc_elapsed_us(long long start, long long end) {
  return (double)(end - start) * qpc_freq_inv_us;
}

static double bench_no_change(int iters) {
  long long t0 = qpc_now();
  for (int i = 0; i < iters; i++)
    lt__bench_sink ^= lt_present();

  long long t1 = qpc_now();
  return qpc_elapsed_us(t0, t1);
}

static void paint_full(int w, int h, lt_uchar fill) {
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
      lt_set_cell(x, y, fill, 0, 0);
}

static double bench_one_cell(int iters) {
  long long t0 = qpc_now();
  for (int i = 0; i < iters; i++) {
    lt_set_cell(0, 0, (i & 1) ? 'A' : 'B', 0, 0);
    lt__bench_sink ^= lt_present();
  }

  long long t1 = qpc_now();
  return qpc_elapsed_us(t0, t1);
}

static double bench_full_repaint(int w, int h, int iters) {
  long long t0 = qpc_now();
  for (int i = 0; i < iters; i++) {
    paint_full(w, h, (i & 1) ? 'X' : 'O');
    lt__bench_sink ^= lt_present();
  }

  long long t1 = qpc_now();
  return qpc_elapsed_us(t0, t1);
}

int main(void) {
  qpc_init();

  if (lt_init() != LT_OK) {
    fprintf(stderr, "lt_init failed\n");
    return 1;
  }

  int w = lt_width();
  int h = lt_height();

  /* warmup: one full repaint to populate front == back */
  paint_full(w, h, ' ');
  lt__bench_sink ^= lt_present();

  double a_us = bench_no_change(BENCH_ITERS);
  double b_us = bench_one_cell(BENCH_ITERS);
  double c_us = bench_full_repaint(w, h, BENCH_ITERS);

  lt_shutdown();

  fprintf(stderr, "bench W=%d H=%d iters=%d (sink=%d)\n",
          w, h, BENCH_ITERS, lt__bench_sink);
  fprintf(stderr, "  (a) no-change       total=%8.1f us  per-frame=%6.2f us\n",
          a_us, a_us / BENCH_ITERS);
  fprintf(stderr, "  (b) one-cell        total=%8.1f us  per-frame=%6.2f us\n",
          b_us, b_us / BENCH_ITERS);
  fprintf(stderr, "  (c) full-repaint    total=%8.1f us  per-frame=%6.2f us\n",
          c_us, c_us / BENCH_ITERS);

  return 0;
}
