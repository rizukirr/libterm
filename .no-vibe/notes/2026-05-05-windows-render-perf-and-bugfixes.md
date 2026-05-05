# Windows render-path bugs + refterm-grade optimizations

Session closed 2026-05-05. 12 layers across 4 curriculum revisions.

## What was built, layer by layer

| # | Layer | What changed | Why it mattered |
|---|---|---|---|
| 1 | Surrogate-state hygiene | Cleared `lt__win_pending_high` on resize-event path and lone-low-surrogate path | A stranded high surrogate would later fuse with an unrelated low half — real bug, untestable today |
| 2 | Output-buffer lifecycle | Added `lt__plat_flush()` after `lt__plat_show_cursor()` in shutdown | Cursor-show escape was being buffered and never flushed; user's terminal never got it |
| 3 | Render-error flush discipline | Flushed in `lt_present` error path before returning | Half-rendered prefix would leak across calls, tail-glued onto next `lt_present` |
| 4 | Diff against front buffer | `lt_present` skips cells where `back == front` (3-field equality) | Headline win — zero bytes on no-change frames; was full repaint every frame |
| 5 | Cursor-position cache | Track last-emitted `(cur_x, cur_y)` in `lt__g`; only emit `\x1b[r;cH` when next changed cell isn't natural advance | Adjacent changed cells now skip the jump — major byte savings on full repaints |
| 6 | Front-buffer sentinel | Initialized front to `0xFFFFFFFF` so first-frame diff was honest | Layer 4 regression — back == front on init meant prior shell scrollback bled through |
| 7 | Transparent unset-cell semantics | `lt_present` skipped emission for `back.ch == 0`; back zero-init | Design exploration — turned out unnecessary once Layer 8 landed; reverted in Layer 9 |
| 8 | Alt-screen toggle | `\x1b[?1049h` on init, `\x1b[?1049l` on shutdown | UX fix — user's prior scrollback preserved across libterm session, restored on exit |
| 9 | Match termbox2 cell defaults (revert L7) | Restored `lt__buffer_clear` on both buffers; removed transparent branch | Alt-screen made L7's workaround unnecessary; aligned with termbox2 semantics |
| 10 | Hand-rolled int-to-decimal writer | Replaced `snprintf` in `lt__plat_move_cursor` with direct digit emission | Refterm-grade — `printf`-family is variable-cost and has no place in render hot path |
| 11 | Surrogate event-loop reset audit | Hoisted clear to top of resize branch (covers 3 returns); added clear before named-key emission (R10) | Finished Layer 1's rule sweep — every event-class boundary clears the latch |
| 12 | Bench harness | `examples/bench_present.c` with three scenarios (no-change, one-cell, full-repaint) | Numbers, not vibes |

### Final bench numbers (W=106, H=55, 5830 cells, 1000 iters)

- **(a) no-change**: 14.56 µs/frame (~2.5 ns/cell — pure diff loop)
- **(b) one-cell**: 27.90 µs/frame (a + ~13 µs marginal — dominated by single WriteFile syscall)
- **(c) full-repaint**: 119.01 µs/frame (~18 ns/changed cell — cursor coalescing + single WriteFile per frame)

8400 fps full-repaint, ~50 Mcells/sec emit throughput.

## Mental model — one paragraph

libterm's Windows render path is a pipeline of bytes from `lt_set_cell` mutations into a back buffer, through a per-frame diff against a front buffer, into a single accumulating output buffer that drains via one `WriteFile` per frame. Three orthogonal optimizations attack three different cost factors: **diff** cuts the *number* of cells that emit (Layer 4); **cursor cache** cuts the *jump bytes per emitted cell* (Layer 5); **direct decimal emission** cuts the *cost of constructing each jump* (Layer 10). They compose multiplicatively. Above all of this sits alt-screen (Layer 8), which is non-negotiable for TUI UX — without it, the library trampolines on whatever the terminal happens to be showing. The bug-class layers (1, 2, 3, 11) all share one shape: **stateful buffers and latches need postconditions stated locally at every exit point, not inferred globally.** The recurring sentinel pattern (`lt__win_pending_high == 0` for "no pair in flight," `(cur_x, cur_y) == (-1, -1)` for "cursor unknown," `front.ch == 0xFFFFFFFF` for "never emitted") is the same idea repeated: a value-from-outside-the-valid-domain that makes "untrustworthy" a first-class state.

## Advanced techniques — pointing outward

- **Direct buffer write instead of `seq[32]` stack scratch.** Refterm's actual hot path writes decimal digits *straight into* the output buffer (`reserve N, write directly, advance length`) rather than into a stack buffer that's then `memcpy`'d. One less copy. Worth a Layer 10b in a future session.
- **SGR fragment cache.** When colors land (M3a SGR rows), cache the rendered `\x1b[...m` bytes per `(fg, bg, attr)` tuple seen this frame. `memcpy` instead of re-formatting. Refterm-grade pattern; trivial table-with-FNV-hash implementation.
- **Damage-region tracking.** `lt_set_cell` could maintain dirty rectangles so `lt_present` skips clean scanlines outright before per-cell diffing. Adds memory pressure but cuts the diff loop's per-frame cost on partial updates.
- **Grapheme cluster segmentation.** UAX #29 essentials (regional indicators, ZWJ sequences, variation selectors, skin-tone modifiers, combining marks) instead of `wcwidth`. One `lt_cell` per cluster, not per codepoint. The refterm reference owns this — currently `wcwidth` is the v0.x fallback per ROADMAP.
- **Alt-screen leak on abnormal exit.** A SIGSEGV / `abort()` mid-program leaves alt-screen and raw mode active. `SetConsoleCtrlHandler` + `atexit` would catch most paths; `__try`/`__except` for SEH would catch crashes. Different session.

## Process gaps logged this session

- **AI miss (ai-note 3)**: Layer 4 introduced a regression because I didn't audit buffer-init invariants before adding the diff. Pattern: any cache-invalidation predicate must hold on init, not just on steady-state transitions.
- **Design contract (ai-note 4)**: user wanted `lt_init` to NOT clear the screen — explored via Layer 7 transparent semantics, ultimately solved cleanly via Layer 8 alt-screen.
- **User typo class (mistake 4)**: `QueryPerformanceCounter` for `QueryPerformanceFrequency` — Win32 name pairs that differ by one trailing word but have unrelated semantics need explicit role-distinction prose before code.
- **User edit-attention class (mistake 5)**: pattern-replace edits across 3+ sites need an explicit checklist of sites by line/function, not just the rule. Attention drops on the third instance.

## What's left for libterm overall (post-session)

Per the user-rewritten ROADMAP.md:
- POSIX side of the entire render path (`plat_init`, `plat_get_size`, `plat_render_cell`, `plat_move_cursor`, `plat_flush` are all no-ops).
- `lt_set_output_mode` still ignored by SGR emission — color rendering not yet implemented (any platform).
- Mouse handling on both platforms.
- Print/send helpers (`lt_print`, `lt_printf`, `lt_send`).
- UTF-8 helpers (`lt__*` exists, public `lt_*` versions not exposed).
- Grapheme clustering (M3b row).
- Direct-buffer-write refactor for the snprintf replacement (refterm-grade next-mile).

This session covered Windows-side render-path correctness + perf, alt-screen UX, and surrogate-stream hygiene. Roadmap M3a is largely complete on the Windows side; M3b is partial (one-syscall flush ✓, hand-rolled int writer ✓, others pending).
