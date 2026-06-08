# Phase 0 Research: Fixed-Outline Floorplanning

**Spec**: [spec.md](./spec.md) · **Plan**: [plan.md](./plan.md) · **Source of truth**: [`scorer/lib/floorplanning.py`](../../../../../scorer/lib/floorplanning.py) (constitution R6)

No `NEEDS CLARIFICATION` items remain — the spec is fully determined by the scorer, the README baseline table, and the input grammar. This document records the **design decisions** (algorithm, geometry, parallelism, fallback) that drive `tasks.md` and implementation.

**Instance facts** (measured from the testcases — they shape every decision below):

| case | chip W×H | soft | fixed | nets | (soft+fixed) area / chip | note |
|------|---------:|-----:|------:|-----:|------:|------|
| sample  | 8×7              | 2  | 2  | 3   | 89.3% | tiny demo, no threshold |
| public1 | 11267×10450      | 15 | 5  | 45  | 77.9% | few **huge** blocks (area up to 20M); ref 581s |
| public2 | 2300×2300        | 16 | 8  | 39  | **93.2%** | **dense** — legality is the hard part |
| public3 | 2500×3000        | 28 | 14 | 108 | 69.8% | most modules/nets; many DOF |
| public4 | 4995×4407        | 20 | 8  | 47  | 65.7% | medium, loose |

Two regimes to be robust across: **dense legality** (`public2`) and **huge-coordinate / few-block** (`public1`), plus **many-DOF wirelength** (`public3/4`).

---

## 1. Problem classification

**Decision**: Treat as **fixed-outline floorplanning with deformable soft modules and fixed obstacles**, objective = **weighted HPWL on module centers**. Variables: per soft module an integer position `(x,y)` and an integer shape `(w,h)`. Constraints: in-outline, `w·h ≥ area`, `0.5 ≤ h/w ≤ 2`, pairwise non-overlap (soft+fixed). It is a small but non-convex continuous-integer placement problem (the non-overlap constraint makes it combinatorial).

**Rationale**: The scorer reads only soft-module rectangles, derives every center, and sums `weight·(|Δcx|+|Δcy|)` over nets — so the deliverable is exactly "shape + place the soft modules legally to minimize weighted HPWL." Module counts are tens, so we can afford expensive per-module work and many restarts.

**Alternatives considered**:
- *Exact / ILP placement with non-overlap (big-M disjunctions)*: optimal but the disjunctive non-overlap blows up; not reliable within 600s even at n≈28. Rejected.
- *Pure analytical (quadratic) placement*: solves centers ignoring overlap, but legalizing arbitrary-size deformable blocks into a tight fixed outline is itself the hard sub-problem. Kept only as an **initializer flavor** (§6a), not the whole method.

---

## 2. Objective = PURE weighted HPWL (the key fix vs. reference)

**Decision**: Optimize **wirelength only**. Area, outline, and aspect are **feasibility constraints**, never cost terms.

**Rationale**: The reference cost is `alpha·total_area + beta·wirelength` with `alpha=beta=0.5`. The scorer's metric is pure weighted HPWL; total area is irrelevant to the score (it is only bounded by the outline). Half the reference's "budget" therefore pushes modules to shrink rather than to shorten wirelength, which both wastes search and biases shapes away from wirelength-optimal ones. Removing the area term is the single largest, lowest-risk win and explains much of the Reference→Min gap (§11).

**Subtlety**: smaller area *indirectly* eases packing (more slack to move modules closer), so we don't forbid shrinking — we just don't reward it. Shape is chosen to serve wirelength/packing, not to minimize area.

**Alternatives**: keep a tiny area/whitespace tie-breaker (`epsilon·area`) only to break exact-WL ties deterministically — optional, must be ≪ any WL difference. Rejected from the main cost.

---

## 3. Geometry: grid-free rectangle model (the scalability fix)

**Decision**: Represent every module as an integer rectangle `(x,y,w,h)` and operate purely with rectangle arithmetic — **no `H×W` grid**.
- **Overlap** (exact scorer match): rectangles `a,b` overlap iff `ax < bx+bw && bx < ax+aw && ay < by+bh && by < ay+ah`. Strict `<` → shared edges (zero-area touch) are **legal**. Pairwise test over ≤42 rectangles is `O(n²)` ≈ ≤ ~1.7k comparisons.
- **In-outline**: `x≥0 && y≥0 && x+w≤W && y+h≤H`.
- **Bottom-left compaction** (replaces the reference's grid `compact`): to drop module `m` down, `new_y = max(0, max over rectangles r that horizontally overlap m of (r.y + r.h))`; then slide left similarly. `O(n)` per module, `O(n²)` for a full compaction sweep.

**Rationale**: The reference allocates `vector<vector<bool>>(H, W)` — **≈117M cells for `public1`** — and pays `O(footprint)` per placement/overlap/compaction op (huge for 20M-area blocks). With ≤42 rectangles the grid buys nothing: rectangle math is exact, matches the scorer's predicate bit-for-bit, and is orders of magnitude faster, so far more SA iterations and restarts fit in budget. This is what makes `public1` cheap.

**Alternatives**: coarse grid (loses exactness vs. the integer scorer); interval/segment trees for overlap (overkill at n≈42). Rejected.

---

## 4. Shape selection with **guaranteed** legality

**Decision**: For a soft module with minimum area `A`, generate a small set (≤ ~32) of candidate integer shapes `(w,h)` with `w·h ≥ A` and `0.5 ≤ h/w ≤ 2`, **and verify the ratio on every candidate**:
- sweep `w ∈ [⌈√(A/2)⌉, ⌊√(2A)⌋]`, set `h = ⌈A/w⌉`;
- if the `⌈⌉` rounding makes `h/w > 2` (rare edge), bump `w`; always include the near-square `w = h = ⌈√A⌉` (ratio 1, always legal).

**Rationale**: A near-square always satisfies both constraints, so a legal shape exists for every module; the candidate set gives the optimizer room to trade aspect for packing/wirelength. **Reshaping moves the center** (`x+w//2`), so shape is a genuine optimization variable, not just a feasibility detail. Note the reference computes `min_width = ceil(sqrt(min_area/2))` with **integer** `min_area/2`, which can (for small/odd areas) emit shapes slightly outside `[0.5,2]`; we explicitly verify each candidate against the scorer's `1e-9`-tolerant ratio test so we never emit an illegal shape.

**Alternatives**: continuous aspect optimization then round (rounding can break the ratio or area — must re-verify anyway); single fixed near-square (legal but gives the optimizer no shaping freedom). Rejected as primary.

---

## 5. Initial placement (legal start; US1 / P1)

**Decision**: **Constructive bottom-left packing**, grid-free. Mark fixed modules as obstacles; place soft modules in an order (area-descending by default; neighbor-centroid and random orders as multi-start variants), each at the lowest-then-leftmost legal position found by scanning candidate y/x against existing rectangles (using the §3 compaction primitive), picking a legal shape that fits. If a module cannot be placed, restart the packing with a different order/shape choice (cheap, since grid-free).

**Rationale**: Guarantees a legal `.floorplan` immediately → satisfies US1/SC-001 and gives every optimization layer a feasible seed. Area-descending bottom-left is the standard robust feasibility heuristic and is what carries the dense `public2` (93%); the reference's analogous grid packer works, and ours is the same idea without the grid cost.

**Alternatives**: random scatter then legalize (often infeasible at high density); shelf/row packing (a fine multi-start variant, kept as one seed flavor). 

---

## 6. Local optimization (quality; US2 / P2)

Two complementary engines, run together (memetic):

**(a) Weighted-median coordinate descent** — **Decision**: repeatedly, for each soft module, compute its wirelength-optimal center as the **weighted median** of the centers of its net-neighbors (separately in x and y; weights = net weights), then try to move the module so its center reaches that target — clamp to the outline, snap to integers, and if the direct move overlaps, compact toward the target / take the closest legal position along the direction. Accept iff wirelength does not increase. Sweep until no module improves.

- **Rationale**: For the L1 (Manhattan) objective with pins at centers, the single-module optimum is exactly the weighted median of its neighbors — an analytical, parameter-free intensifier that converges fast and is the largest quality lever the reference lacks. Independence in x/y comes straight from `|Δcx|+|Δcy|`.

**(b) Simulated annealing** — **Decision**: Metropolis SA over moves {**translate** to a nearby/random legal spot, **swap** two modules' positions (reshaping to fit), **reshape** within the candidate set, **nudge toward median**}, geometric cooling, **pure-WL** cost, **incremental HPWL** (recompute only the moved module's incident nets), legality via §3 checks. Track and restore the best legal solution.

- **Rationale**: SA escapes the median descent's local optima and handles the discrete swap/reshape moves; combining global SA with periodic median intensification beats either alone. Pure-WL cost + incremental eval keeps the inner loop hot, so the tiny instances run millions of moves.

**Alternatives**: B*-tree / sequence-pair SA (guaranteed non-overlap, textbook for floorplanning) — **rejected as primary** because **fixed modules at arbitrary positions** break their compaction/packing model (they assume freely-packable blocks), and adapting them (constraint graphs with position pins, or fixed-block penalties) is complex and error-prone for a shared, multi-model spec. Absolute coordinates + the §3 compaction give the same non-overlap guarantee with native fixed-obstacle support. Force-directed spreading alone (no SA) — weaker on swaps/reshape.

---

## 7. Parallel multi-start (R2; US3 / P2+P3)

**Decision**: Run **M independent chains** (each = init + SA + median descent) **in parallel via OpenMP** (`#pragma omp parallel for`, optionally `std::thread`), each with a distinct seed and diversified init order / starting temperature; **reduce to the best legal** result (lowest wirelength; tie-break lowest chain index). M scales with core count and the remaining time budget.

**Rationale**: SA + median descent are local-optimum-prone; diverse restarts are the cheapest large quality gain and are embarrassingly parallel → the direct R2 fulfilment. The Min targets sit 1.4–2.2× below the single-start reference (§11), so diversification is essential to reach them. Grid-free + tiny n means each chain is cheap, so M can be large.

**Alternatives**: single start (reference — too weak for Min); one shared SA state with parallel move proposals (contention, non-deterministic). Rejected.

---

## 8. Incremental HPWL & in-memory layout

**Decision**:
- Per **soft module**: `x,y,w,h`, candidate-shape list, and a list of incident net ids. Per **fixed module**: `x,y,w,h` and a **precomputed constant center** (never moves).
- Per **net**: `(endpointA, endpointB, weight)` as module ids (soft or fixed) + a flag for which endpoints are fixed.
- Maintain a running total wirelength; a single-module move recomputes only that module's incident nets (`O(deg)`), updating the total by the delta.

**Rationale**: `n` and `M` are tiny (M≤108), so even full recompute is cheap — but incremental update keeps the SA inner loop minimal and makes the median descent's accept/reject test `O(deg)`. Precomputing fixed centers removes redundant work (fixed endpoints are constants in the median computation too). No heavy parsing layer is needed (files are ≤157 lines); a simple `getline`+tokenize or whole-buffer scan suffices.

**Alternatives**: net-bbox / multi-pin HPWL structures — unnecessary, every net here is exactly 2-pin (`Net A B weight`), so HPWL per net is just `weight·(|Δcx|+|Δcy|)`.

---

## 9. Time budget & determinism

**Decision**: Capture a wall-clock start (`omp_get_wtime()` / `std::chrono::steady_clock`) and set an internal deadline **< 600s** (e.g. ~575–590s) checked in the SA and multi-start loops; on expiry, finalize the **best legal solution so far** and write it. Make stopping primarily **iteration/restart-budget** driven, tuned to converge comfortably under the deadline, with the deadline only as a safety truncation. Per-chain seed = chain index; the best-result reduction is tie-broken by `(wirelength, chain index)`.

**Rationale**: The README budget is ~600s and the scorer additionally enforces a 1200s subprocess hard timeout; finishing under 600s with margin for output writing is required (SC-005). Always having a legal best-so-far protects R3 even if truncated. Iteration-budget stopping + fixed seeds make re-runs reproducible (FR-015); truncation keeps a monotone best-so-far, so a re-run is never worse. The reference uses `clock()` (CPU time) at 580 — wrong under multithreading (it sums all cores); we use **wall-clock**.

**Alternatives**: `clock()` CPU-time (breaks with OpenMP); no deadline (risks DNF and over-600s on `public1`). Rejected.

---

## 10. Matching scorer semantics exactly (R6)

**Decision**: Mirror [`floorplanning.py`](../../../../../scorer/lib/floorplanning.py) precisely:
- **Output only soft modules**, every one exactly once; fixed modules are obstacles/anchors read from the input and are **never** written.
- **Format**: optional first line `Wirelength <v>` (scorer reads it case-insensitively as self-report), then `NumSoftModules <n>`, then `n` lines `<name> <x> <y> <w> <h>` with the module **name as the first token**. Blank lines are ignored by the scorer; extra trailing tokens/lines are ignored.
- **Centers**: `(x + w//2, y + h//2)` with integer **floor** division — compute internal wirelength the same way (C++ integer `/` on non-negative coordinates equals floor) so the self-report equals the scorer's value (FR-010/SC-006).
- **Legality**: in-outline; `w·h ≥ area` (area is a **lower bound**); `h/w ∈ [0.5, 2]` within `1e-9`; strict-inequality non-overlap over soft+fixed.
- **Integers only** in the output.

**Rationale**: By R6 any divergence from the scorer is a bug regardless of "real" correctness. Two-pin nets and floor centers must match exactly or the self-reported `Wirelength` will disagree with the score.

---

## 11. R1 Baseline Fallback — **no de-Boosting needed**

**Decision**: If self-written code is worse than reference on **all** cases (R1 trigger), copy `reference/src/main.cpp` into `experiments/<model>/`, build with `g++ -std=c++20 -O3 -fopenmp -pthread`, and optimize on top — the highest-value edits being: (1) switch its cost to **pure wirelength** (drop the `alpha·area` term), (2) wrap its SA in a **parallel multi-start**, (3) replace the giant grid with rectangle checks if time permits.

**Rationale**: Unlike the 001 reference, the **002 reference uses no Boost** — all includes are standard (`<vector>`, `<unordered_map>`, …) and the Makefile is just `-O3 -lm`. So the fallback is a near-zero-friction port (mainly: build with the project flags and add OpenMP). It is a safety net rarely expected to fully trigger, because the pure-WL + median-descent + multi-start design should beat the single-start, area-burdened reference on at least one case.

**Alternatives**: rewrite reference from scratch (defeats the purpose of a fallback). Rejected.

---

## 12. Targets & expected gap (from README)

| testcase | Min (≤ goal) | Reference | Reference runtime | Max (zero-score) | Reference/Min |
|----------|-------------:|----------:|------------------:|-----------------:|------:|
| public1  | 161,609,972  | 239,984,392 | 581.41s | 349,768,634 | 1.49× |
| public2  | 20,966,863   | 38,494,434  | 45.86s  | 41,569,628  | 1.84× |
| public3  | 1,856,276    | 2,621,582   | 111.55s | 5,045,921   | 1.41× |
| public4  | 63,024,850   | 137,686,350 | 285.35s | 201,625,050 | 2.18× |

**Implication**: beating **Reference** (the R1 bar) is very achievable — the reference wastes half its cost on area, runs a single start, and does no analytical wirelength optimization. Reaching **Min** (SC-003) is harder on the high-gap cases: `public4` (2.18×) and the dense `public2` (1.84×) are the toughest; `public1`'s few huge blocks make wirelength dominated by a handful of placements (good for median descent); `public3`'s many DOF reward multi-start. The plan layers L1→L3 so each increment is independently legal and scored, and Min is approached via L3 (parallel multi-start + median intensification).
