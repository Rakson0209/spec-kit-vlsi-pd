# Implementation Plan: Fixed-Outline Floorplanning

**Date**: 2026-06-08 | **Spec**: [spec.md](./spec.md)

## Technical Context

**Language/Version**: C++20
**Compiler**: `g++ -std=c++20 -O3 -fopenmp -pthread` (portable mingw64 g++ 16.1.0; load via `. .\tools\mingw64\setup-env.ps1`, or the winget WinLibs g++ already on User PATH).
**Primary Dependencies**: C++ standard library + **OpenMP** (`-fopenmp`) + `std::thread`/pthread (`-pthread`). **No Boost** (constitution v0.6.0 R2; the 002 reference is itself Boost-free — its Makefile is only `-O3 -lm`).
**Platform**: Windows native (mingw64). **Exec constraint**: an unsigned, self-compiled `.exe` is blocked (`Access is denied`, Defender ASR/EDR) outside `D:\FSecret\`; build/copy the binary into `D:\FSecret\` to run it, while output files may live anywhere (see [quickstart.md](./quickstart.md)).
**Scale**: tiny instances — `sample` (2 soft/2 fixed/3 nets) → `public3` (28 soft/14 fixed/108 nets); ≤ ~42 total rectangles, ≤108 nets. But chip outlines are large (up to `11267×10450`) and one case is dense (`public2` ≈ **93.2%** utilization). The work is **optimization-bound, not parse-bound**: the human reference spends up to **581s**. Internal wall-clock deadline set **< 600s**.
**Determinism**: same input re-run yields equal-or-better wirelength and identical legality (FR-015); each parallel chain's seed = chain index, and the best-result reduction is tie-broken by `(wirelength, chain index)` → reproducible. Primary stopping is an iteration/restart budget; the wall-clock deadline only truncates extra restarts (keeping best-so-far, which is monotone).
**Unknowns**: none. No `NEEDS CLARIFICATION` — legality and the metric are fully determined by [`scorer/lib/floorplanning.py`](../../../../../scorer/lib/floorplanning.py) (constitution R6), the README baseline table, and the input grammar.

> ⚠️ **Template correction**: `plan-template.md` still carries stale Boost references (`-I tools/boost`, `boost::graph/geometry`, an R2 worded as "Boost + 平行"). This plan follows the **approved constitution v0.6.0** (R2 = OpenMP + pthread, **no Boost**) and overrides the template's Technical Context and Constitution Check accordingly. For 002 this is doubly safe because the reference needs no Boost at all.

## Approach Summary

The problem is **fixed-outline floorplanning of deformable (soft) modules around fixed obstacles**, minimizing **weighted HPWL with pins at module centers**. The instance is *small* (≤28 soft modules) but the objective landscape is non-convex because of the hard non-overlap + outline + per-module area/aspect constraints. Three first-principles observations drive the design and the three deltas vs. the reference:

1. **The objective is pure wirelength.** The reference minimizes `0.5·area + 0.5·wirelength`, but area is **not** in the scorer's objective — only wirelength is. Spending half the optimization budget shrinking area directly inflates the metric. **We optimize pure weighted HPWL**; area/outline/aspect are *constraints*, enforced for legality, never in the cost. (Largest single source of the Reference→Min gap.)
2. **No grid is needed.** With ≤ ~42 rectangles, overlap is an `O(n²)` pairwise test and bottom-left **compaction is `O(n²)` rectangle arithmetic** — the reference's explicit `H×W` boolean grid (≈ **117M cells** for `public1`) is pure overhead. Dropping it makes every case cheap and lets us run vastly more iterations/restarts.
3. **L1 HPWL has an analytical local optimum.** For one movable module with pins at centers, the wirelength-minimizing center is the **weighted median** of its net-neighbors' centers, separable in x and y. This gives a fast coordinate-descent intensifier the reference entirely lacks.

Layered to match the three user stories (each layer independently shippable & scored):

- **L1 (US1 / P1, MVP "legal")**: robust parser → guaranteed-legal **shape selection** (integer `w·h ≥ area`, `0.5 ≤ h/w ≤ 2`) → **constructive bottom-left packing** of soft modules into the free space around fixed modules (grid-free), guaranteeing non-overlap + in-outline. Emits a **legal** `.floorplan`, passes the scorer (SC-001). Robust even at `public2`'s 93% density via area-descending placement.
- **L2 (US2 / P2, "beat Reference")**: **pure-wirelength local optimization** — (a) **weighted-median coordinate descent** (snap each module's center toward its neighbors' weighted median, then legalize) + (b) **simulated annealing** over moves {translate, swap, reshape-within-ratio, nudge-to-median} with **incremental HPWL** (only the moved module's incident nets) and `O(n²)` legality checks. Pure-WL cost + analytical intensification clears the Reference bar (R1).
- **L3 (US2+US3 / P2+P3, "reach Min within budget")**: **OpenMP parallel multi-start** — M independent (init + SA + median-descent) chains with diverse seeds/orders/temperatures across cores; reduce to the **best legal** result. M scales with cores × remaining budget. This is the embarrassingly-parallel quality lever and the direct R2 fulfilment; targets the per-case **Min**.

Decisions in [research.md](./research.md); structures in [data-model.md](./data-model.md); interface contracts in [contracts/](./contracts/); validation in [quickstart.md](./quickstart.md).

## Constitution Check (v0.6.0)

| Rule | Design mapping | Status |
|------|----------------|:---:|
| **R1** Baseline Fallback | Write the grid-free pure-WL solver → score all 5 cases; if **every** case is worse than `reference/src/` → copy `reference/src/main.cpp` into `experiments/<model>/` (it is **Boost-free** — trivial port) and optimize on top (switch its cost to pure WL; parallel multi-start). Keep self-written code if it beats Reference on **any** case. See research §11. | ✅ planned |
| **R2** Parallel-first (OpenMP + pthread, **no Boost**) | L3 parallel multi-start across cores; optional parallel median-descent / cost eval; flags `-fopenmp -pthread`; zero Boost dependency. | ✅ planned |
| **R3** Legality hard gate | L1 constructive packing guarantees a legal start; every SA/median move is accepted only if it stays in-outline, non-overlapping, area- and ratio-legal; final write is always a legal solution. | ✅ planned |
| **R4** All-case gate | quickstart scores all 5 testcases in one command; re-run after every code change. | ✅ planned |
| **R5** Spec-first | spec → plan → tasks → code; this is the plan phase — no implementation code written. | ✅ |
| **R6** Scorer is truth | Legality and wirelength follow `scorer/lib/floorplanning.py` exactly: strict-inequality overlap (edge-touch legal), integer **floor** centers `(x+w//2, y+h//2)`, area as a lower bound, `h/w ∈ [0.5,2]` within `1e-9`. | ✅ |

**Gate result**: no violations → Complexity Tracking left empty.

## Project Structure

Phase 2 models each implement in their own directory (single translation unit, for clean per-model comparison):

```
problems/002-floorplanning/experiments/<model>/
├── main.cpp        # parser + shape gen + constructive pack + median descent + SA + multi-start + writer
├── Makefile        # g++ -std=c++20 -O3 -fopenmp -pthread -o hw3 main.cpp
├── spec/           # copied from opus spec/ (this plan.md / research.md / tasks.md …)
└── out/            # <case>.floorplan (sample, public1..4)
```

> The planning directory (this `experiments/claude-opus-4-8/spec/`) holds only SDD documents; opus's own implementation lives at `experiments/claude-opus-4-8/main.cpp`.

## Phase Outputs

- **Phase 0 → [research.md](./research.md)**: problem classification, the pure-WL objective decision, grid-free geometry & compaction, guaranteed-legal shape generation, constructive init, weighted-median descent, SA move set & incremental HPWL, parallel multi-start, time budget/determinism, exact scorer-semantics matching, R1 fallback (no de-Boosting needed), and the target-gap analysis.
- **Phase 1 → design artifacts**: [data-model.md](./data-model.md), [contracts/cli-contract.md](./contracts/cli-contract.md), [contracts/io-format.md](./contracts/io-format.md), [quickstart.md](./quickstart.md).
- **Phase 2 (`/speckit.tasks`)**: generate dependency-ordered `tasks.md` from this plan.

## Complexity Tracking

> No constitution violations — section intentionally empty.

| Violation | Why | Rejected Alternative |
|-----------|-----|----------------------|
| —         | —   | —                    |
