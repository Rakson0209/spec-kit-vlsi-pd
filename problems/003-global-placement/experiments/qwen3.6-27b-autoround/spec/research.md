# Research: Global Placement (HPWL Minimization)

**Date**: 2026-06-08 | **Spec**: [spec.md](./spec.md) | **Plan**: [plan.md](./plan.md)

All decisions below resolve to **no remaining NEEDS CLARIFICATION**. Legality and the metric are fully fixed by the pure-Python scorer ([`scorer/lib/placement.py`](../../../../../scorer/lib/placement.py), [`bookshelf.py`](../../../../../scorer/lib/bookshelf.py), [`legalize.py`](../../../../../scorer/lib/legalize.py)) per constitution R6, plus the README baseline table.

---

## §0. Baseline targets (the bar to beat)

| testcase | cells | nets | pins | rows | **Min (≤ target)** | Reference | Ref runtime | Max (zero) | Ref/Min ratio |
|----------|------:|-----:|-----:|-----:|------------------:|----------:|-----------:|-----------:|-----:|
| public1  | 12,028 | 11,507 | 44,266 | 132 |  **59,788,412** |  87,987,694 |  28.43s |    319,198,465 | 1.47× |
| public2  | 29,347 | 28,446 | 126,308 | 148 | **10,530,075** | 18,642,174 |  58.95s |     28,999,635 | 1.77× |
| public3  | 51,382 | 50,393 | 187,872 | 246 | **395,131,978** | 750,902,922 | 110.7s | 2,631,834,205 | 1.90× |

`public2` carries **1201 terminals (fixed cells)**; `public1`/`public3` have none (all cells movable). Runtime ceiling **~590s**. The reference finishes in 28–111s — it leaves **5×–20× of the time budget unused**, the single largest exploitable slack.

---

## §1. Problem classification

**Decision**: Treat this as **flat analytical (force-directed) global placement** minimizing HPWL, with a **density (spreading) penalty** to keep the placement legalizable.

**Rationale**: The cells are single-row-height standard cells (confirmed by `legalize.py`'s premise and verified on all 3 cases). HPWL is a separable-per-axis half-perimeter objective. The dominant modern method for this exact problem (Bookshelf standard-cell HPWL minimization) is **quadratic/nonlinear analytical placement** (NTUPlace3 / RePlAce / SimPL lineage): model wirelength with a smooth differentiable surrogate, add a density term that pushes cells apart, and minimize with a gradient solver while ramping the density weight. This is precisely what the course reference implements.

**Alternatives considered**:
- *Min-cut / recursive partitioning* (Capo-style): legal-by-construction but historically higher HPWL and more code; rejected as primary.
- *Pure simulated annealing on positions*: hopeless at 51k cells in 590s; rejected.
- *Keep initial `.pl`*: the initial `.pl` is all `(0,0)` (collapsed, out-of-core) — fails legality immediately; not an option.

---

## §2. The scorer's anti-collapse trap (most important design driver)

**Decision**: The objective must be **HPWL minimization *subject to* a spreading constraint**; we never optimize HPWL alone.

**Rationale**: HPWL is scored on the **overlapping** placement ([placement.py:97](../../../../../scorer/lib/placement.py)), so naively a global pile gives near-zero HPWL. But [legalize.py](../../../../../scorer/lib/legalize.py) spreads the output onto `.scl` rows with single-row Tetris and measures average displacement; if `avg_disp > 0.05 × min(coreW, coreH)` (or the run aborts at `0.15×`), the case is **NG** ([placement.py:108-122](../../../../../scorer/lib/placement.py)). Empirically (scorer docstring) a real spread solution legalizes at `≈0.006–0.011×`, a collapsed one at `≥0.15×` — a ~15× gap.

**Consequence for the algorithm**: the density/spreading term is not optional polish — it is what makes the solution *legal*. A correctly converged analytical placement (cells distributed to ~uniform bin density) legalizes with tiny displacement and passes by a wide margin. We must drive the density penalty until the placement is genuinely spread, then stop — over-spreading needlessly inflates HPWL, under-spreading fails the health check. The `0.05×` limit gives ~5× headroom over a well-spread solution, so the target is comfortably reachable.

**Alternatives considered**: optimizing pure WL then legalizing only at output — rejected, because the *scored* metric is the global (pre-legalize) HPWL; we need the global placement itself to be both low-HPWL and spread.

---

## §3. Wirelength model

**Decision**: Use a smooth HPWL surrogate — **Weighted-Average (WA)** model preferred, **Log-Sum-Exp (LSE)** as the simpler fallback — with per-axis separability, and a smoothing parameter `γ` scaled to the core size (start `γ ≈ coreW/10` like the reference, anneal down).

**Rationale**: True HPWL `Σ (max−min)` is non-differentiable. LSE approximates `max xᵢ ≈ γ·log Σ e^{xᵢ/γ}` (and `min` via `−γ·log Σ e^{−xᵢ/γ}`); the reference uses exactly this. WA (`Σ xᵢ e^{xᵢ/γ} / Σ e^{xᵢ/γ}`) is a tighter, better-conditioned estimator of HPWL at the same γ and is the modern default (NTUPlace3/RePlAce). Smaller γ → closer to true HPWL but stiffer gradients; annealing γ from large→small refines the estimate as the placement settles.

**Scorer alignment**: the scored HPWL uses **pin offsets** `(x+xoff, y+yoff)` ([bookshelf.py:138-150](../../../../../scorer/lib/bookshelf.py)). Internal WL gradients are computed on module centers; the constant pin offset shifts a pin's position but not its gradient w.r.t. the module center, so optimizing center-based WL is equivalent up to constants. The **final reported/validated** HPWL must be recomputed with offsets to match the scorer exactly.

**Net weights**: `.wts` is **not applied** by the scored metric ([compute_hpwl](../../../../../scorer/lib/bookshelf.py) ignores weights). Decision: optimize **unweighted** WL (weights may be read but left at 1.0 in the objective) so the surrogate tracks the scored quantity.

**Alternatives considered**: quadratic (B2B / star) wirelength — converges fast via linear solves but is a looser HPWL proxy; usable for a quick initial placement but not as the final objective. Rejected as primary; optionally usable for init (§5).

---

## §4. Density (spreading) model

**Decision**: **Bell-shaped bin-density penalty** (the reference's smoothing) on a **size-adaptive bin grid**, with the density weight `λ` ramped up across outer iterations (penalty / Lagrangian schedule). Target bin density = total movable area / core area.

**Rationale**: A differentiable density term is required for gradient spreading. The bell-shaped smoothing kernel (piecewise-quadratic θ in x and y) gives continuous gradients pushing cells out of over-full bins — directly reduces the legalize displacement that the health check measures.

**Key delta vs reference — adaptive bin resolution**: the reference hard-codes `binCut = 14` → **196 bins** regardless of design. For 51k cells that is ~260 cells/bin — far too coarse to resolve local overcrowding, leaving cells piled (risking the health check *and* inflating post-spread HPWL). Decision: scale bins so each bin spans a small constant number of rows / so `#bins ≈ Θ(#cells)` (e.g. bin width ≈ a few sites, bin height ≈ 1–4 rows; or `binCut ≈ √(area/ (k·avgCellArea))`). Finer bins → smoother, more accurate spreading → lower legalized displacement *and* lower achievable HPWL. The runtime cost of more bins is offset by §6 parallelism and the large unused time budget.

**λ schedule**: start `λ = 0` (one WL-only solve to get a wirelength-driven layout), set an initial `λ` from the first density gradient, then multiply up each outer round (reference's `Increase_Lambda`). Stop when the placement is sufficiently uniform (max bin density ≈ target, equivalently estimated legalize displacement under the `0.05×` bar) — this is the natural convergence/stop test tied directly to SC-006.

**Alternatives considered**: electrostatics (ePlace/RePlAce Poisson density via FFT) — state-of-the-art and would beat Min comfortably, but a correct FFT density solver is substantial code; held as a stretch option if the bell-shaped penalty stalls above Min. Rejected as the default for first implementation.

---

## §5. Initial placement

**Decision**: Replace the reference's `randomPlace` with a **wirelength-aware initial placement**: place all movable cells at the **core center**, then run a few WL-only (λ=0) gradient iterations (cells connected to fixed terminals get pulled toward them; clusters form), *or* a quick quadratic (star-model) solve. Then begin the density ramp.

**Rationale**: Random init wastes early iterations untangling noise and is non-deterministic in quality. A center/WL-driven start converges faster and more reproducibly (FR-013). For `public2`, the 1201 terminals anchor the field, so a WL-first pass meaningfully pre-positions connected cells.

**Alternatives considered**: keep random init (reference) — rejected (slower, non-deterministic). Bound-to-Bound quadratic + force iterations (SimPL) — strong but more machinery; the WL-only warmup captures most of the benefit cheaply.

---

## §6. Performance & parallelism (R2)

**Decision**: Compile `g++ -std=c++20 -O3 -fopenmp -pthread`. **OpenMP-parallelize the objective/gradient hot loops**: the per-net WL accumulation (loop over nets), the per-bin density accumulation (loop over bins × cells), and the per-cell gradient scatter. Use **thread-local accumulators + reduction** to avoid races on shared gradient/bin arrays. Optionally run a **parallel multi-start** (a few chains with different γ/seed/bin schedules) and keep the best **legal** result.

**Rationale**: For 51k cells × 50k nets × (adaptive) bins, each FG evaluation is the cost center and is embarrassingly data-parallel. The reference is single-threaded and still finishes in ≤111s; with parallel FG and the full 590s budget we can afford finer bins, smaller γ, and many more CG iterations — the levers that close the Ref→Min gap. Parallel multi-start is the direct R2 fulfilment and a quality hedge.

**Determinism (FR-013)**: parallel reductions over floating point are order-sensitive; to keep legality reproducible we (a) use fixed thread counts and deterministic reduction order (e.g., index-ordered tree reduction or per-thread partial sums combined in fixed order), and (b) seed any multi-start chain by its index, tie-breaking the best-result pick by `(scorer-HPWL, chain index)`. Minor FP nondeterminism in HPWL is acceptable as long as legality never flips; the spreading margin (§2) ensures the health check is not on a knife's edge.

**Alternatives considered**: GPU/FFT — out of scope for the toolchain. Single-thread (reference) — violates R2; rejected.

---

## §7. Legality enforcement at output

**Decision**: After analytical convergence, **clamp** every movable cell's center so its bounding box lies inside the core (`xmin ≤ x`, `x+w ≤ xmax`, same in y) before converting center→lower-left for output. Do **not** legalize-to-rows in our tool (the scorer scores the global placement and runs its own legalizer for the health check).

**Rationale**: FR-004 requires in-core lower-left bounding boxes; clamping guarantees it without perturbing the spread (the density term already keeps cells off the boundary mostly). The health check (FR-006/SC-006) is satisfied by §2/§4 convergence, not by us snapping to rows — and snapping ourselves could *raise* the scored global HPWL. We optionally snap cell x to the site grid / y to the nearest row only if it measurably lowers the scorer's legalize displacement without hurting HPWL.

**Coordinate conventions** (must match scorer exactly):
- `.pl` `(x,y)` = **lower-left**; analytical var = **center**; convert `lowerleft = center − (w/2, h/2)` on output, `center = lowerleft + (w/2,h/2)` on read.
- Pin global = `(cell.x + xoff, cell.y + yoff)`, offsets from `.nets`, relative to lower-left.
- HPWL = `Σ_nets (max−min)_x + (max−min)_y`, **unweighted** ([compute_hpwl](../../../../../scorer/lib/bookshelf.py)).
- Fixed = `terminal` in `.nodes` **OR** `FIXED` in `.pl`; never moved (FR-005).
- Core = from `.scl` rows: `(min SubrowOrigin, min Coordinate, max(SubrowOrigin+NumSites·Sitespacing), max(Coordinate+Height))`; **may be negative** ([parse_core](../../../../../scorer/lib/bookshelf.py)).

---

## §8. Output format

**Decision**: Emit Bookshelf `.gp.pl`: a `UCLA pl 1.0` header line, then one line per **movable** cell `<name>   <x>   <y> : N`. Optionally also emit fixed cells at their exact input coords (allowed only if unchanged). Coordinates may be real numbers; the scorer's `parse_pl` reads the first three tokens as name/x/y.

**Rationale**: matches [parse_pl](../../../../../scorer/lib/bookshelf.py) (skips `#`/`UCLA`/blank lines, needs `name x y` then `:`); movable coverage is mandatory (FR-003/FR-008), fixed defaults from base `.pl` so omitting them is safe.

---

## §9. Time-budget control

**Decision**: Wall-clock guard at **~560s** (margin under the 590s ceiling): the outer λ-ramp loop checks elapsed time each round and exits early, always keeping the best spread-and-clamped placement so far (monotone: we only accept rounds that keep the placement legal/spread). Inner CG iteration counts scale with remaining time and design size.

**Rationale**: FR-011/SC-005. The reference's fixed `i<5, 150/30 iters` underuses the budget; a time-driven loop spends the slack on more density rounds / smaller γ → lower HPWL, while the guard prevents timeout on `public3`.

---

## §10. R1 Baseline Fallback — feasibility analysis (critical, differs from 001/002)

**Finding**: `reference/obj/*.o` (`BookshelfParser`, `Placement`, `Wrapper`, `NumericalOptimizer`, `Util`) are **Linux ELF x86-64 relocatable objects** (`file` confirms; project memory confirms). The Windows portable mingw `g++` **cannot link them**, and the official `verify` is a Linux ELF executable. Therefore the literal R1 action — *copy `reference/src/*.cpp` and build* — is **not buildable on this Windows-native toolchain**, because the reference compiles only against those opaque precompiled Linux objects.

**Decision**: The implementation is **fully self-contained** — our own Bookshelf parser, our own data model, our own WL+density objective, and our own gradient/CG optimizer — with **zero dependency on `reference/obj`**. The R1 fallback is reinterpreted (consistent with R1's intent — "don't lose to baseline") as: **port the reference *algorithm*, not its binary**. The reference's recipe is fully visible in [`GlobalPlacer.cpp`](../../../../../problems/003-global-placement/reference/src/GlobalPlacer.cpp) and [`ExampleFunction.cpp`](../../../../../problems/003-global-placement/reference/src/ExampleFunction.cpp): LSE wirelength + bell-shaped bin density + conjugate-gradient with a `λ` (beta) ramp. If our self-written placer is worse than the README **Reference** numbers on **all** cases, we fall back to reproducing that exact recipe (same γ, same bell kernel, same λ schedule) inside our self-contained code, then optimize on top (finer bins §4, WL-aware init §5, parallel FG §6, full time budget §9). We keep the self-written variant as long as it beats Reference on **any** case (constitution R1).

**Why self-written can beat Reference**: the gap is not algorithmic novelty but the four under-tuned reference knobs — coarse fixed bins (§4), random init (§5), single-thread (§6), and a tiny fixed iteration budget (§9). Fixing those within 590s is the path from Reference (1.47×–1.90× over Min) to ≤ Min.

---

## §11. Verification strategy (R3/R4)

**Decision**: After every code change, run `python scorer/score.py 003 --output-dir <out>` over **all 3** public cases; require `valid=OK` on each and record HPWL, runtime, and the `note` field's `avgDisp` (legalizability margin) vs the Min/Reference/Max table. The pure-Python scorer is the sole arbiter (R6); the official Linux `computeHpwl`/`verify` are not used here.

**Rationale**: R3 (all legal) + R4 (all-case gate) + R6 (scorer truth). The `avgDisp` note is the early-warning signal for the anti-collapse health check (§2) — watch it stays well under `0.05×`.

---

## Decisions summary

| # | Decision | Rationale (one line) |
|---|----------|----------------------|
| 1 | Flat analytical placement (WL surrogate + density penalty) | Standard, scalable to 51k cells, matches reference family |
| 2 | Optimize HPWL **subject to spreading** | Scorer's anti-collapse health check rejects piles (§2) |
| 3 | WA (or LSE) wirelength, γ ≈ coreW/10 annealed | Smooth differentiable HPWL proxy; WA tighter than LSE |
| 4 | Bell-shaped density, **adaptive bins**, λ ramp | Reference's 14×14 bins too coarse; finer = better spread + HPWL |
| 5 | WL-aware (center + λ=0 warmup) init | Faster, deterministic vs random init |
| 6 | OpenMP-parallel FG + optional multi-start | R2; uses unused time budget for finer search |
| 7 | Clamp-to-core on output; don't self-legalize | FR-004; preserves scored global HPWL |
| 8 | Bookshelf `.gp.pl`, movable cells covered | Matches scorer `parse_pl` (§7/§8) |
| 9 | ~560s wall-clock guard, keep best-so-far | FR-011; spend reference's unused slack |
| 10 | Self-contained code; R1 = port algorithm not binary | reference `.o` are Linux ELF, unlinkable on Windows (§10) |
| 11 | Score all 3 cases every change, watch avgDisp | R3/R4/R6 (§11) |
