# Phase 0 Research: Multi-Technology Die Partitioning

**Spec**: [spec.md](./spec.md) · **Plan**: [plan.md](./plan.md) · **Source of truth**: [`scorer/lib/partitioning.py`](../../../../../scorer/lib/partitioning.py) (constitution R6)

No `NEEDS CLARIFICATION` items remain — the spec is fully determined by the scorer, the README baseline table, and the input grammar. This document records the **design decisions** (algorithm, data layout, parallelism, fallback) that drive `tasks.md` and implementation.

---

## 1. Problem classification

**Decision**: Treat as **balanced hypergraph bipartitioning for min-cut** with **two-sided area-capacity constraints** and **side-dependent vertex weights**.

**Rationale**: Cells = vertices, nets = hyperedges, objective = number of hyperedges spanning both parts (exactly the scorer's `cut`). The only feasibility constraints are the two per-die utilization caps (no separate balance ratio — see spec Assumptions). The twist vs. classic FM/hMETIS: a cell's area depends on the die's Tech, so vertex weight is `area_A` on DieA and `area_B` on DieB. This maps cleanly onto the FM/multilevel family with a per-side weight function.

**Alternatives considered**:
- *ILP / exact min-cut*: optimal but infeasible at 10⁶ vertices within 300s. Rejected.
- *Spectral partitioning*: needs eigen-solves on huge sparse matrices; heavy dependency, poor fit for hard caps. Rejected.
- *Pure simulated annealing*: too slow to converge to Min-level cuts on large hypergraphs. Rejected as primary (kept as optional tie-breaker only).

---

## 2. Core algorithm — area-constrained Fiduccia–Mattheyses (FM)

**Decision**: Use **FM local search** as the refinement engine, with move-eligibility gated by the destination die's area cap.

**Gain model** (net-cut, identical to scorer's cut definition and to `reference/src` `Gn`): for cell `c` on its *from* side F with *to* side T, over each incident net `n` let `F(n)` = #cells of `n` on F, `T(n)` = #cells on T. Moving `c`:
- **uncuts** net `n` (+1) iff `F(n) == 1` (c is the last cell on F) and `T(n) ≥ 1`;
- **cuts** net `n` (−1) iff `T(n) == 0` before the move (all cells on F) and `deg(n) ≥ 2`.

So `gain(c) = (#incident nets with F(n)==1) − (#incident nets with T(n)==0)`. Incremental updates touch only the cells of nets whose `F/T` crossed 0/1 after a move (standard FM neighbor update).

**Area feasibility**: a move A→B is allowed iff `areaB + area_B(c) ≤ capB·(W·H)` (with `+1e-9` tolerance to match scorer `≤`). The source side only frees area, so it never blocks. On move: `areaFrom -= area_From(c)`, `areaTo += area_To(c)`.

**Pass structure**: each FM pass repeatedly extracts the max-gain *unlocked, feasibly-movable* cell, moves+locks it, updates neighbor gains and net counts, and tracks the **cumulative-gain prefix maximum**; at pass end it **rolls back** to the best prefix (so a pass never worsens the cut). Repeat passes until improvement stalls (best-prefix gain ≤ 0 or ≤1 move) or the deadline hits.

**Rationale**: FM is near-linear per pass (`O(pins)`) with bucket gains, is the proven workhorse for VLSI min-cut, and the `≤cap` (vs reference's strict `<`) gives slightly more feasible headroom → equal-or-better moves than reference.

**Alternatives**: Kernighan–Lin (O(n²) per pass — rejected at scale); single greedy gain pass only (what reference largely does — insufficient to beat Min).

---

## 3. Initial partition

**Decision**: **Feasibility-first greedy** — iterate cells; assign each to the die where it is *relatively cheaper* (`area_A/capA` vs `area_B/capB`) if that die's cap still allows, else the other die; track running `areaA/areaB`. For multi-start, randomize cell order and tie-breaking per seed.

**Rationale**: Guarantees a legal starting point whenever one exists (testcases are feasible by construction), satisfying US1/SC-001 immediately and giving FM a balanced, capacity-aware seed. Mirrors the reference's relative-cost heuristic but parameterized for restarts.

**Alternatives**: random assignment (often infeasible against tight caps); area-descending bin-packing (kept as one of the multi-start variants).

---

## 4. Multi-start, parallel (R2)

**Decision**: Run **K independent (init + FM) restarts in parallel** via OpenMP (`#pragma omp parallel for`) and/or `std::thread`, each with a distinct seed; reduce to the **best legal** (lowest cut) result. K scales with core count and remaining time budget.

**Rationale**: FM is local-optimum-prone; restarts from diverse seeds are the cheapest large quality gain and are embarrassingly parallel → direct fulfilment of R2. The Min targets are 2–5.6× below the single-start reference (e.g. public5 297 vs 1669), so diversification is essential.

**Alternatives**: single start (reference — too weak); ILP/portfolio of exact solvers (infeasible at scale).

---

## 5. Multilevel partitioning (large cases)

**Decision**: For `|cells|` above a threshold (≈ 50k), wrap FM in a **multilevel** scheme: **coarsen** (cluster strongly-connected cells via heavy-edge / first-choice matching, accumulating per-Tech area) into a hierarchy → **initial partition** at the coarsest level (greedy + multi-start FM) → **uncoarsen** projecting the partition down, running **FM refinement at each level**.

**Rationale**: This is the hMETIS/KaHyPar recipe that produces Min-class cuts and runs in near-linear time — necessary for both quality and the ~300s budget on `public3/5/6`. Flat FM alone on millions of vertices is both slower to good solutions and quality-limited. Coarsening also shrinks the search space so more multi-starts fit in budget.

**Alternatives**: flat FM only (acceptable MVP for small cases, weak on large); recursive bisection (degenerate here — only 2 parts).

**Risk/scope note**: multilevel is the most complex piece. Weaker models may ship only L1+L2 (flat multi-start FM); R1 then decides per-case whether their result beats reference. The plan deliberately layers so a partial implementation is still legal and scored.

---

## 6. Gain data structure

**Decision**: **Bucket-list gains** — an array indexed by gain value in `[−maxDeg, +maxDeg]`, each slot a doubly-linked list of cells; maintain a pointer to the current max non-empty bucket. `O(1)` max extraction and `O(1)` per-neighbor gain update.

**Rationale**: Standard FM acceleration; avoids the reference's `std::map<gain,list>` + `priority_queue` overhead (log factor + per-move bucket rebuilds). Gains are bounded by max net degree, so a flat array (offset by `maxDeg`) is cache-friendly and fast.

**Alternatives**: balanced BST / heap of gains (reference uses these — `O(log n)` and heavier); recompute-all-gains each pass (reference rebuilds buckets per pass — acceptable but costly at scale).

---

## 7. I/O and in-memory data layout

**Decision**:
- **Parsing**: read the whole file into a buffer and tokenize manually (hand-rolled integer/word scan), not `getline`+`istringstream` per line. **Intern cell names → contiguous `int` ids** via a reserved `unordered_map<string,int>` (or open-addressing map); keep an `id→name` table for output.
- **Storage**: **CSR-style flat arrays** for both directions — `cellPins` (cell→net ids) and `netPins` (net→cell ids) as `values[] + offsets[]` — rather than `vector<vector<>>`. Per cell store `area_A`, `area_B`, `side`, `locked`, `gain`. Per net store `countA`, `countB`.

**Rationale**: `public6` is ~4.15M lines (millions of pins). The reference's stream tokenization + `unordered_map<string,…>` keyed by name on every access is the dominant cost at scale. Buffered parse + int ids + CSR cut both time and memory and improve cache locality — required for FR-010/FR-011.

**Alternatives**: memory-mapped I/O (faster still, but Windows `mmap` portability cost — buffered read is enough); `vector<vector>` adjacency (simpler but heavier allocation/cache misses at 10⁶ scale).

---

## 8. Time budget & determinism

**Decision**: Track a **wall-clock deadline ≈ 285s** (`std::chrono::steady_clock` captured at start); check it in the FM inner loop and the multi-start loop; on expiry, finalize the **best legal partition found so far** and write output. Multi-start seeds are a deterministic function of restart index, and the best-result reduction breaks ties by lowest cut then lowest index → reproducible (FR-013).

**Rationale**: scorer enforces a 1200s subprocess timeout but the README budget is ~300s; 285s leaves margin for output writing. Always-have-a-legal-answer protects R3 even under timeout.

**Alternatives**: `clock()` CPU-time (reference uses 280s `clock()` — wrong under multithreading, counts all cores); no deadline (risks DNF on `public6`).

---

## 9. Matching scorer semantics exactly (R6)

**Decision**: Mirror the scorer precisely:
- **Coverage**: emit every cell exactly once; never emit unknown/duplicate names.
- **Area**: cell area = `width×height` of its LibCell **under that die's Tech**; `utilX = areaX/(W·H)`; legal iff `utilX ≤ capX + 1e-9`.
- **Cut**: a net counts once iff its cells touch both A and B.
- **Output**: optional `CutSize <n>` first line (scorer reads it case-insensitively as self-report), then `DieA <countA>` + `countA` name lines, then `DieB <countB>` + `countB` name lines; cell name is the **first token** of each line; counts must equal the listed lines.

**Rationale**: Any divergence from the scorer is, by R6, a bug regardless of "real" correctness. Self-reported `CutSize` must equal the actual partition's cut (FR-007/SC-006).

---

## 10. R1 Baseline Fallback — porting `reference/src` off Boost

**Decision**: If self-written code is worse than reference on **all** cases (R1 trigger), copy `reference/src/main.cpp` into `experiments/<model>/` and **port it off Boost** before building:
- Replace `#include <boost/unordered_map.hpp>` → `#include <unordered_map>`; drop `using namespace boost;`.
- Remove the unused `#include <boost/fusion/...>` lines.
- Add the missing std headers it relies on transitively (`<unordered_map>`, `<map>`, `<list>`).
- Build with `g++ -std=c++20 -O3 -fopenmp -pthread` (drop the Makefile's `-I /usr/local/include/boost/`).
- Then optimize on top (e.g. wrap its FM in a parallel multi-start) to clear the per-case bar.

**Rationale**: The reference depends on Boost, which is unavailable here (memory `boost-unavailable`); the only Boost use is `boost::unordered_map` (swap-in compatible with `std::unordered_map`) plus dead `fusion` includes. The reference is single-start and took ~225s on `public6`; a correct parallel multi-start FM should beat it on ≥1 case, so the fallback is a safety net rarely expected to fully trigger.

**Alternatives**: vendoring Boost headers (`boost.zip` is corrupt — memory); rewriting reference from scratch (defeats the purpose of a fallback).

---

## 11. Targets & expected gap (from README)

| testcase | Min (≤ goal) | Reference | Max (zero-score) | Min/Ref gap |
|----------|-------------:|----------:|-----------------:|------------:|
| public1  | 104          | 193       | 1,441            | 1.9× |
| public2  | 816          | 3,666     | 27,862           | 4.5× |
| public3  | 1,762        | 7,092     | 103,659          | 4.0× |
| public4  | 982          | 2,265     | 12,421           | 2.3× |
| public5  | 297          | 1,669     | 48,964           | 5.6× |
| public6  | 5,159        | 10,281    | 490,120          | 2.0× |

**Implication**: beating Reference (R1 gate) is achievable with solid multi-start FM; reaching **Min** (SC-003) on the large, high-gap cases (public2/3/5) realistically needs **multilevel**. Plan layers L1→L3 so each increment is independently shippable and scored.
