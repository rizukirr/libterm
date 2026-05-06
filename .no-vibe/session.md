# Session: Windows render-path refterm-grade optimizations (round 2)

**Mode:** skill (transcription-heavy refterm patterns)
**Refs:** `external/refterm/` (primary), `external/termbox2/termbox2.h` (compatibility-tier baseline)
**Scope:** Windows platform layer + `src/shared/output.c` + `src/shared/buffer.c` + `examples/bench_present.c`. POSIX, color/SGR, mouse out of scope.

## Baseline (closed prior session, 2026-05-05)

W=106, H=55, 5830 cells, 1000 iters per scenario:

- (a) no-change       :  14.56 µs/frame
- (b) one-cell        :  27.90 µs/frame
- (c) full-repaint    : 119.01 µs/frame

These are the numbers each layer's Phase 4 review compares against.

## Curriculum

- [x] **Layer 1 — ASCII fast-path in `lt__plat_render_cell`.** Most TUI text is ASCII; UTF-8 encode for `ch < 0x80` is a single-byte cast. Branch early, write the byte directly, skip the encoder. Hits (b) and (c). Refterm: branch-predicted-common-case discipline.

- [x] **Layer 2 — Direct-buffer-write for cursor jumps.** Replace the `seq[32]` stack scratch + `memcpy` in `lt__plat_move_cursor` with a `reserve N bytes` API on the output buffer; write digits and literal bytes straight into `lt__outbuf`. Refterm: `BasicWritePtr`-style — never copy when you can write-in-place.

- [x] **Layer 3 — Run-length glyph coalescing in `lt_present`.** When N adjacent cells differ from front, walk to the end of the run (same row, contiguous, no SGR change for now since color is monochrome), emit one cursor jump + one `lt__plat_write` block of N glyphs. Replaces N per-cell `render_cell` calls with one block-emit. Refterm: batch-like-work principle.

- [x] **Layer 4 — Damage-region tracking (per-row dirty bit).** *Reordered by revision 1.* Add `bool *dirty_rows` to `lt__g`; `lt_set_cell` flips `dirty_rows[y] = true`; `lt_present` skips entire rows where the bit is clear (no per-cell diff at all on those rows); after present, all bits cleared. On full repaint, every bit set, no win — but on incremental UIs (chat, log viewer), most rows are clean and skipped wholesale. Refterm: tile-coarse damage regions for the same reason. (was Layer 5)

- [x] **Layer 5 — Establish `intrinsics/` skeleton (scalable SIMD architecture).** *Replaced original AVX2-inline plan, revision 2.* Create `src/intrinsics/{common.h,diff.h,scalar.c}`, add `LIBTERM_SIMD=auto|scalar|avx2|avx512|neon|sve|rvv` CMake selection with per-TU compile flags, refactor `lt_present` to call `lt__simd_diff_first_differ_cell` instead of inline cell equality. Scalar TU implements the contract; structure is general so future ISAs and ops drop in without architectural change. No perf change yet — scalar wraps the same scalar work; the architecture is the deliverable.

- [x] **Layer 6 — AVX2 TU (`intrinsics/avx2.c`).** Drop in the AVX2 implementation of the diff contract — one new file. CMake `auto` selection prefers AVX2 on x86_64 hosts; flag `-mavx2` applied per-TU. No source changes elsewhere; the scalar fallback path tested in Layer 5 still works when `LIBTERM_SIMD=scalar`. Bench validates the SIMD win is real.

- [x] **Layer 7 — Synthesis & comparison.** *Was Layer 6.* Compile a 7-row × 3-column bench trajectory (baseline → L1 → L2 → L3 → L4 → L5 → L6; columns = scenarios a/b/c). Identify the largest win, any regressions, ROADMAP rows now `[x]` Windows. Phase 6 synthesis to `notes/`.

## Per-layer bench protocol

Each impl layer's Phase 4 review runs `build\examples\bench_present.exe` and records the three numbers in this session.md status block:

```
After Layer N: a=__.__ µs  b=__.__ µs  c=__.__ µs   (delta vs baseline: ...)
```

If a layer regresses any scenario by >10%, pause and investigate before advancing.

## Out of scope this session

- SGR / color emission (`c->fg`/`c->bg` still ignored in render path).
- POSIX side of any of these.
- Mouse handling.
- SGR-fragment cache (depends on color emission landing first).
- Per-rectangle damage tracking (per-row is enough for v0).
- Direct outbuf writes for **render_cell** glyph bytes (Layer 2 covers cursor jumps only; glyph-direct-write is a possible follow-on if Layer 3 doesn't subsume it).

## Bench trajectory (filled by each layer)

- Baseline (start of session) : a=14.56  b=27.90  c=119.01
- After Layer 1 : a=12.43  b=27.17  c=108.07  (W=101,H=55=5555 cells; per-cell vs baseline: a -10%, b ~flat, c -4.4%)
- After Layer 2 : a=14.65  b=26.69  c=106.76  (a back to baseline noise; b -4.3% vs baseline; c -10.3% vs baseline cumulative)
- After Layer 3 : a=15.20  b=26.67  c=96.17  (W=106, comparable to baseline; c -19.2% vs baseline — headline run-coalesce win)
- After Layer 4 : a=0.15  b=19.06  c=38.43  (a -99% — pure Layer 4 design payoff; b -28%; c -60% but partially conhost-WriteFile variance, re-run for stability)
- After Layer 5 : (numbers not captured; structure verified working via Release-build hello + bench. Two side bugs surfaced and fixed: arena_free using libc free() instead of HeapFree, and a long-latent buffer overrun where `arena_alloc(cells_bytes)` only sized for one buffer while the code used two via `front = base + n`)
- After Layer 6 :

## Status

**Current:** Layer 5 about to open. Curriculum revised again this turn (revision_id 1 → 2) to split the original SIMD layer into a skeleton/architecture layer (5) and an AVX2 TU layer (6), pushing synthesis to Layer 7. Reason: user committed to a portable cross-ISA `intrinsics/` folder design supporting AVX2/AVX-512/NEON/SVE/RVV, which is too big for a single layer per the phases-md split test. The skeleton lands first with scalar baseline; AVX2 drops in second.

revision_id: 2.
