# Implementation Plan: Global Placement (HPWL Minimization)

**Date**: 2026-06-08 | **Spec**: [spec.md](./spec.md)

## Technical Context

**Language/Version**: C++20
**Compiler**: `g++ -std=c++20 -O3 -fopenmp -pthread` (portable mingw64 g++ 16.1.0; load via `. .\tools\mingw64\setup-env.ps1`, or the winget WinLibs g++ already on User PATH).
**Primary Dependencies**: C++ standard library + **OpenMP** (`-fopenmp`) + `std::thread`/pthread (`-pthread`). **No Boost.** **No link against `reference/obj/*.o`** — those are **Linux ELF x86-64** objects and cannot be linked by the Windows mingw toolchain (research §10). The implementation is **fully self-contained**: own Bookshelf parser, own data model, own WL+density objective, own gradient/CG optimizer.
**Platform**: Windows native (mingw64). **Exec constraint**: an unsigned, self-compiled `.exe` is blocked outside `D:\FSecret\` (Defender ASR / corporate EDR); build/copy the binary into `D:\FSecret\` to run it, while output files may live anywhere (see [quickstart.md](./quickstart.md)).
**Scale**: large flat instances — `public1` 12,028 cells / 11,507 nets / 44,266 pins / 132 rows; `public2` 29,347 cells (incl. **1201 terminals**) / 28,446 nets / 126,308 pins / 148 rows; `public3` 51,382 cells / 50,393 nets / 187,872 pins / 246 rows. The work is **optimization-bound**: the objective/gradient (per-net WL + per-bin density) dominates. Reference finishes in 28–111s, leaving **5×–20× of the ~590s budget unused** — the primary exploitable slack.
**Determinism**: same input re-run yields equal-or-better HPWL and identical legality (FR-013); fixed thread count + fixed-order parallel reductions; multi-start chains seeded by index; best-result tie-broken by `(scorer-HPWL, chain index)`. Spreading margin (avg disp ≪ 0.05×) keeps the legality check off any knife-edge despite FP reduction noise.
**Unknowns**: none. No `NEEDS CLARIFICATION` — legality and the metric are fully determined by [`scorer/lib/placement.py`](../../../../../scorer/lib/placement.py) + [`bookshelf.py`](../../../../../scorer/lib/bookshelf.py) + [`legalize.py`](../../../../../scorer/lib/legalize.py) (R6), the README baseline table, and the Bookshelf grammar.

> ⚠️ **Template correction**: `plan-template.md` still carries stale Boost references (`-I tools/boost`, `boost::graph/geometry`, R2 worded as "Boost + 平行"). This plan follows the **approved constitution v0.6.0** (R2 = OpenMP + pthread, **no Boost**) and overrides the template accordingly. For 003 there is a second, harder reason no external lib is used: the reference's precompiled objects are Linux-only, so the tool must stand alone.

## Approach Summary

The problem is **flat analytical global placement of standard cells** minimizing **HPWL**, where cells may overlap in the scored placement *but* the scorer rejects collapsed piles via a row-Tetris **legalizability health check** (research §2). So the objective is **HPWL minimization subject to a spreading constraint** — modeled, as in the course reference, by a smooth **wirelength surrogate + a density penalty** minimized by gradient descent with a ramping density weight. Four first-principles observations drive the design and the deltas vs. the reference:

1. **Overlap is scored, collapse is illegal.** HPWL is measured on the overlapping placement, so a global pile scores ~0 — but legalizing it would need huge displacement, which the health check (`avg disp ≤ 0.05×min(coreW,coreH)`) forbids. The density term is therefore *what makes the solution legal*, not mere polish. (research §2)
2. **The reference under-tunes four knobs.** Its bins are a fixed **14×14 = 196** regardless of design (≈260 cells/bin at `public3`), its init is **random**, it is **single-threaded**, and it uses a **tiny fixed iteration budget** (i<5, 150/30 CG iters) finishing in ≤111s. The Ref→Min gap (1.47×–1.90×) is closed by fixing exactly these — not by a new algorithm. (research §4/§5/§6/§9)
3. **The metric is unweighted, pin-offset HPWL.** The scorer ignores `.wts` and uses pin offsets `(x+xoff, y+yoff)`. We optimize the *unweighted* surrogate and recompute any reported HPWL with offsets so it matches the scorer exactly. (research §3)
4. **The reference binary is unbuildable here.** `reference/obj/*.o` are Linux ELF; the Windows toolchain can't link them, so R1's "copy reference/src" is infeasible. The implementation is self-contained, and R1 falls back to *porting the reference algorithm* (LSE WL + bell density + λ ramp — fully visible in source) into our own code. (research §10)

Layered to match the three user stories (each layer independently shippable & scored):

- **L1 (US1 / P1, MVP "legal")**: robust Bookshelf parser (`.aux→.nodes/.nets/.pl/.scl/.wts`) → flat index-based data model (CSR nets, fixed-cell marking, core/row bounds) → a **constructive legal spreading** (distribute movable cells across the core/rows so the placement is in-core, fixed untouched, and trivially passes the anti-collapse health check). Emits a **legal** `.gp.pl`, passes the scorer (SC-001) — even at high HPWL. This is the minimum viable, legality-guaranteed deliverable.
- **L2 (US2 / P2, "beat Reference")**: **analytical placement** — smooth wirelength model (**WA** preferred, LSE fallback; γ ≈ coreW/10 annealed) + **bell-shaped bin-density penalty** with a **λ ramp** (WL-only warmup → increase λ each round), minimized by our own **conjugate-gradient** solver. WL-aware init (center + λ=0 warmup) replaces random init. Drives HPWL ≤ Reference then toward Min while the density term keeps the placement spread (legal). Clamp centers in-core on output.
- **L3 (US2+US3 / P2+P3, "reach Min within budget")**: **OpenMP-parallel objective/gradient** (per-net WL, per-bin density, gradient scatter with thread-local reduction) + **size-adaptive bin resolution** (finer than 14×14) + **wall-clock-guarded λ ramp** (~560s, keep best-so-far) + optional **parallel multi-start** (diverse γ/seed), reduce to best **legal** result. This is the embarrassingly-parallel quality lever (R2) and the direct path to the per-case **Min** by spending the reference's unused time slack.

Decisions in [research.md](./research.md); structures in [data-model.md](./data-model.md); interface contracts in [contracts/](./contracts/); validation in [quickstart.md](./quickstart.md).

## Constitution Check (v0.6.0)

| Rule | Design mapping | Status |
|------|----------------|:---:|
| **R1** Baseline Fallback | Reference `obj/*.o` are **Linux ELF**, unlinkable on Windows (research §10) → "copy reference/src" is **infeasible**. Reinterpreted per R1 intent: write a self-contained analytical placer → score all 3 cases; if worse than README **Reference** on **every** case → re-implement the reference *algorithm* (LSE WL + bell density + λ ramp, visible in `GlobalPlacer.cpp`/`ExampleFunction.cpp`) inside our own code and tune on top. Keep self-written if it beats Reference on **any** case. | ✅ planned (variant) |
| **R2** Parallel-first (OpenMP + pthread, **no Boost**) | L3 OpenMP-parallel FG (nets/bins/gradient) with deterministic reduction; optional parallel multi-start; flags `-fopenmp -pthread`; zero Boost / zero `reference/obj`. | ✅ planned |
| **R3** Legality hard gate | L1 constructive spreading guarantees a legal start; the density term keeps every later placement spread; output clamps all movable bboxes in-core, never moves fixed; final write is always legal (incl. anti-collapse health). | ✅ planned |
| **R4** All-case gate | quickstart scores all 3 testcases in one command; re-run after every change; watch `valid` + `avgDisp` note. | ✅ planned |
| **R5** Spec-first | spec → plan → tasks → code; this is the plan phase — no implementation code written. | ✅ |
| **R6** Scorer is truth | Legality & HPWL follow the pure-Python scorer exactly: lower-left coords, pin offsets `(x+xoff,y+yoff)`, **unweighted** per-net half-perimeter, in-core within `1e-6`, fixed = terminal∨FIXED, anti-collapse disp `≤0.05×core`. Official Linux `computeHpwl`/`verify` not used. | ✅ |

**Gate result**: R1's literal action is infeasible on this platform; the documented variant (self-contained + algorithm port) preserves R1's intent. Logged in Complexity Tracking; no other violations.

## Project Structure

Phase 2 models each implement in their own directory (single translation unit, for clean per-model comparison):

```
problems/003-global-placement/experiments/<model>/
├── main.cpp        # parser + data model + constructive spread + WL/density objective + CG + parallel multi-start + writer
├── Makefile        # g++ -std=c++20 -O3 -fopenmp -pthread -o hw4 main.cpp   (NO reference/obj link)
├── spec/           # copied from opus spec/ (this plan.md / research.md / tasks.md …)
├── out/            # <case>.gp.pl (public1..3)
└── RESULT.md       # per-case valid / HPWL / runtime / rounds (template in experiments/README.md)
```

> The planning directory (this `experiments/claude-opus-4-8/spec/`) holds only SDD documents; opus's own implementation lives at `experiments/claude-opus-4-8/main.cpp`.

## Phase Outputs

- **Phase 0 → [research.md](./research.md)**: baseline targets, problem classification, the anti-collapse trap, WL model (WA/LSE), density model & adaptive bins, init, parallelism & determinism, output legality/clamp, time budget, the R1/Linux-ELF feasibility analysis, and verification strategy.
- **Phase 1 → design artifacts**: [data-model.md](./data-model.md), [contracts/cli-contract.md](./contracts/cli-contract.md), [contracts/io-format.md](./contracts/io-format.md), [quickstart.md](./quickstart.md).
- **Phase 2 (`/speckit.tasks`)**: generate dependency-ordered `tasks.md` from this plan.

## Complexity Tracking

| Violation | Why | Rejected Alternative |
|-----------|-----|----------------------|
| R1 literal action ("copy `reference/src` and build") not performed | `reference/obj/*.o` are Linux ELF x86-64; Windows mingw cannot link them (research §10, confirmed by `file` + project memory). Building the reference requires Linux/WSL, which this machine lacks. | Attempt Linux/WSL build — rejected: no WSL on this corporate Windows host; comparison must run on the Windows-native, pure-Python scorer (R6). Variant chosen: self-contained code + port the reference *algorithm* (not its binary), preserving R1's "don't lose to baseline" intent. |
