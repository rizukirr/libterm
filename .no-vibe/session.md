# Session: Windows render-path bugs + refterm-grade optimizations

**Mode:** debug (L1–3) → skill (L4–6, L9–12) → concept (L7–8)
**Refs:** `external/termbox2/termbox2.h`, `external/refterm/`
**Scope:** Windows platform layer + `src/shared/output.c` + `src/shared/buffer.c`. POSIX, color/SGR, decode stub out of scope. Alt-screen pulled IN by revision 3.

## Curriculum

- [x] **Layer 1 — Surrogate-state hygiene.**
- [x] **Layer 2 — Output-buffer lifecycle.**
- [x] **Layer 3 — Render-error flush discipline.**
- [x] **Layer 4 — Diff against front buffer.**
- [x] **Layer 5 — Cursor-position cache.**
- [x] **Layer 6 — Front-buffer sentinel for first-frame full-repaint.**
- [x] **Layer 7 — Transparent unset-cell semantics.** *Will be reverted in Layer 9; kept marked complete because it informed the design exploration.*
- [x] **Layer 8 — Alt-screen toggle.**

- [x] **Layer 9 — Match termbox2 cell-default semantics (revert Layer 7).** *Inserted by revision 4.* Now that alt-screen (Layer 8) solves the "don't clobber user's scrollback" UX problem, Layer 7's transparent-`ch==0` semantics are no longer load-bearing — they only contribute the "can't un-emit a rendered cell" limitation. Match termbox2: cells default to `(' ', clear_fg, clear_bg)`. Two reverts: (a) restore the `lt__buffer_clear` calls on both back and front in `lt__buffer_resize` (replace the `memset(back, 0, ...)` and the front-sentinel loop) — both buffers initialized to space-cells matches `init_cellbuf` at termbox2 line 3169-3170; (b) remove the `if (bc->ch == 0) { ... continue; }` transparent branch from `lt_present` in `output.c`. After this, the first `lt_present` after `lt_init` is a wire-bytes no-op (back == front everywhere), and alt-screen handles the visual cleanliness.

- [x] **Layer 10 — Hand-rolled int-to-decimal writer (refterm-grade).** Replace `snprintf("\x1b[%d;%dH", ...)` in `lt__plat_move_cursor` with direct decimal-into-buffer writes. (was Layer 9)

- [x] **Layer 11 — Surrogate event-loop reset audit.** Re-read `plat_input.c` end-to-end. (was Layer 10)

- [x] **Layer 12 — Bench harness + before/after numbers.** `examples/bench_present.c` timing N×`lt_present` on 200×60. (was Layer 11)

## Out of scope this session

- SGR / color emission in `render_cell` (`c->fg`/`c->bg` stay ignored).
- POSIX side of any of these.
- `lt__utf8_decode` stub.
- `SetConsoleCP(CP_UTF8)` cargo audit.
- `lt__win_in` shared accessor refactor.

## Status

**Current:** Layer 9, just opened. Curriculum revised (revision 3 → 4) to revert Layer 7's transparent-default semantics now that alt-screen carries the UX requirement Layer 7 was hand-rolling. Front-buffer sentinel from Layer 6 also gets undone — termbox2 doesn't use one, both buffers init to identical space-cells, the visual cleanliness comes from alt-screen, not from forcing a first-frame full repaint.

revision_id: 4.
