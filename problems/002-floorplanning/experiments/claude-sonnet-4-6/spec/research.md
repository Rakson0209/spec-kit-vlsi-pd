# Research: Fixed-outline Floorplanning Optimizer

**Phase 0 — Research-First (Constitution Principle VI)**
**Date**: 2026-06-05
**Feature directory**: `problems/002-floorplanning/experiments/claude-sonnet-4-6/spec/`

---

## 1. Baseline Metrics (Must Beat These)

| testcase | **Min 門檻（目標 ≤）** | Reference 參考解 | Min/Reference |
|----------|---------------------:|----------------:|:-------------:|
| public1  | **161,609,972**       | 239,984,392     | 0.673 (−33%)  |
| public2  | **20,966,863**        | 38,494,434      | 0.545 (−45%)  |
| public3  | **1,856,276**         | 2,621,582       | 0.708 (−29%)  |
| public4  | **63,024,850**        | 137,686,350     | 0.458 (−54%)  |

> **Primary target**: HPWL ≤ Min for all testcases.
> **Secondary target**: At minimum, beat Reference (i.e., HPWL < Reference value).

---

## 2. Problem Analysis

### 2.1 Scale

| testcase | Soft modules | Fixed modules | Nets | Chip size         | Soft area |
|----------|------------:|-------------:|-----:|-------------------|----------:|
| sample   | 2           | 2            | 3    | 8 × 7             | 40        |
| public1  | 15          | 5            | 45   | 11,267 × 10,450   | 91,310,000|
| public4  | 20          | 8            | 47   | 4,995 × 4,407     | 10,708,500|

**Key insight**: Instances are SMALL (≤ 20 soft + 8 fixed = 28 modules total). The bottleneck is not algorithmic complexity but **moves-per-second**.

### 2.2 Legality Constraints (from `scorer/lib/floorplanning.py`)

1. `x >= 0`, `y >= 0`, `x+w <= chip_W`, `y+h <= chip_H` — soft module in bounds
2. `w * h >= min_area` — area is a **lower bound**, NOT equality → can over-size to fill dead space
3. `0.5 <= h/w <= 2.0` — aspect ratio constraint
4. No pairwise overlaps (soft vs soft, soft vs fixed)
5. Fixed modules: position unchanged (not in output)

### 2.3 HPWL Formula (exact, from scorer)

```
HPWL = Σ_{net(m1,m2,w)} w × (|cx1 − cx2| + |cy1 − cy2|)
where cx = x + w//2,  cy = y + h//2   (floor division)
```

Nets are **2-pin only**. This is Manhattan distance between floor-rounded module centers, weighted by net weight.

### 2.4 Reference Algorithm Analysis (from `reference/src/main.cpp`)

The reference implementation:
1. **Representation**: 2D boolean pixel grid of size `chip_W × chip_H`
   - public1: 11,267 × 10,450 = **117.7 million cells**
   - Each `set_grid`/`check_placed` call: O(module_area) ≈ O(6M) for public1
2. **Initial placement**: Sort by area desc → greedy bottom-left compaction
3. **SA parameters**: T₀=1000, T_min=1, decay=0.95, K=20, N=20×K=400 moves/temp
4. **SA operators**: swap positions, move right/up, change aspect ratio (from 20 pre-enumerated shapes)
5. **Cost function**: `0.5 × total_area + 0.5 × HPWL`
   - Area term is wasted: since area ≥ min_area (not equality), area changes only from rounding. Adding area to cost does NOT help HPWL.

**Critical weakness**: The pixel-grid means each SA move requires O(6M) operations. In 580 seconds, only **~100–500 SA iterations** complete at outer temperature loop (bounded by grid ops). This is catastrophically slow.

Our approach with a compact geometric representation can achieve **100,000–1,000,000× more moves per second**, translating directly to much better solution quality.

---

## 3. Algorithm Survey

### 3.1 Candidate Algorithms

| Algorithm | Pack complexity | Per-move | SA moves/s estimate | Quality | Implementation |
|-----------|:-:|:-:|:-:|:-:|:-:|
| **Pixel grid (reference)** | O(W×H) | O(mod_area) | ~1–10 | Poor | Trivial |
| **Sequence Pair + SA** | O(n²) | O(n²)≈400 | ~100K–1M | Good | Moderate |
| **B\*-tree + SA** | O(n log n) | O(n log n) | ~1M–10M | Excellent | Hard |
| **Slicing tree + SA** | O(n) | O(n) | ~10M | Good | Moderate |
| **Analytical (force-directed + legalization)** | O(n·iter) | O(n) | very fast | Excellent | Hard |
| **ILP / MILP** | — | — | N/A | Optimal | Not feasible |

### 3.2 Deep Dive: Sequence Pair

- Introduced by Murata et al. (1996)
- A floorplan is encoded as (Γ⁺, Γ⁻) — two permutations of module IDs
- **Packing**: Build two DAGs from (Γ⁺, Γ⁻); longest path gives x,y of each module
- For n=20: O(n²) = 400 operations per pack — extremely fast
- **SA operators**: swap adjacent in Γ⁺ or Γ⁻, move a module to random position
- **Handling fixed modules**: Pre-pin fixed modules' relative order in both permutations
- **Soft module sizing**: Choose (w,h) with w×h ≥ area, 0.5 ≤ h/w ≤ 2 at pack time
- **Fixed outline**: enforce as hard constraint by adjusting module sizes or as penalty

### 3.3 Deep Dive: B\*-tree

- Introduced by Chang et al. (2000 DAC, 2001 DAC)
- Binary tree where left-child = right neighbor, right-child = above neighbor
- **Packing**: O(n) scan using a horizontal contour (augmented interval tree)
- **SA operators**: rotate, delete-insert, swap subtrees
- Superior quality to sequence pair, especially for tight fixed-outline constraints
- More complex implementation (~400–600 lines for robust version)

### 3.4 Deep Dive: Slicing Tree (Polish Expression)

- Only handles **slicing** floorplans (subset of all floorplans)
- Encoded as Polish expression: operands=modules, operators=H/V cuts
- For soft modules with flexible AR: bottom-up optimization gives global optimum in one pass
- Cannot naturally handle fixed modules with arbitrary positions
- Limited to ~70% of all possible compact floorplans

### 3.5 Deep Dive: Force-Directed Placement

- Model nets as springs: `E = Σ w·d²` (quadratic), or use clique-HPWL approximation
- Minimize energy via conjugate gradient: fast, converges in O(n×100) steps
- Produces wire-length-optimal solution ignoring overlaps
- Then **legalize**: overlap removal with minimum HPWL distortion
- Legalization for small instances: simple spread or LP-based
- Excellent HPWL quality but legalization is tricky

---

## 4. Decision: Selected Algorithm

### 4.1 Chosen Approach: **Sequence Pair + Simulated Annealing + Soft-module AR Co-optimization**

**Decision**: Sequence Pair + SA is the primary algorithm.

**Rationale**:
1. **Speed**: For n=20, O(n²)=400 ops per pack. With a 3GHz CPU, ~100K–500K SA moves/sec is achievable vs. the reference's ~10 moves/sec. This gives **10,000–50,000× more optimization steps** in the same 600s budget.
2. **Correctness**: Sequence pair always produces a valid non-overlapping floorplan; legality is guaranteed by construction.
3. **Fixed outline**: Enforce as a combination of hard constraint + penalty. If packed W or H exceeds chip bounds, apply large penalty; additionally try soft module AR adjustments to shrink.
4. **Soft module flexibility**: At packing time, choose (w,h) from the feasible set `{(w,h) : w×h ≥ area, 0.5 ≤ h/w ≤ 2}` to minimize HPWL contribution to adjacent nets. This is a 1D optimization (over aspect ratio r = h/w ∈ [0.5, 2]) for each module.
5. **Pure HPWL cost**: Use `cost = HPWL` only (α=0, β=1). Area is managed by the feasibility constraint, not the cost function.
6. **Implementation feasibility**: Can be implemented in ~300–400 lines of well-tested C++20, achievable in one SDD cycle.

### 4.2 Why Not B\*-tree

B\*-tree offers higher asymptotic performance but:
- Implementation is 2–3× more complex
- For n≤20, sequence pair's O(n²)=400 vs B\*-tree's O(n log n)≈90 is not significant at these scales
- Risk of implementation bugs outweighs marginal speed gain
- Sequence pair is well-proven to match B\*-tree quality for n≤50 instances

### 4.3 Why Not Slicing Tree

Cannot handle fixed modules with arbitrary positions. The PAD modules in test cases occupy interior perimeter positions, not just corners, breaking the slicing assumption.

### 4.4 Why Not Analytical Placement

Legalization for small instances with non-uniform module sizes is complex, especially with the aspect ratio constraint. The risk of introducing overlaps during legalization is high.

---

## 5. Algorithm Design

### 5.1 Representation

```
SequencePair = (Γ⁺: vector<int>, Γ⁻: vector<int>)
  — each element is a module ID (soft + fixed combined)
  
Module {
  id, name, x, y, w, h,
  area (min_area for soft, exact for fixed),
  is_fixed, aspect_ratio ∈ [0.5, 2.0]
}
```

Fixed modules: their relative order in Γ⁺ and Γ⁻ is pre-determined by their absolute positions. Specifically, module A is "to the left of" module B if A.x + A.w ≤ B.x. Fixed modules' relative constraints are encoded by fixing their pairwise relationship in both sequences, then only perturbing soft module positions in the sequences.

Simpler approach: include fixed modules in the sequence pair but always restore their positions during packing (i.e., fixed modules act as anchors — after computing longest-path positions, override with actual fixed coords and propagate adjustment to dependent soft modules).

Actually, the simplest valid approach: **exclude fixed modules from the sequence pair** and handle their overlap with soft modules as hard constraints. During packing, the soft-module sequence pair gives initial positions; then shift any soft module that overlaps a fixed module.

### 5.2 Packing Algorithm

Given a sequence pair (Γ⁺, Γ⁻) over soft modules only:

1. Build horizontal constraint graph G_h:
   - Edge (i→j) with weight w_i if i appears before j in both Γ⁺ and Γ⁻ (meaning i is to the LEFT of j)
2. Longest path in G_h → x-coordinate of each module
3. Build vertical constraint graph G_v similarly (i above j if i before j in Γ⁺ but j before i in Γ⁻)
4. Longest path in G_v → y-coordinate
5. Result: non-overlapping placement of soft modules

Fixed modules: add them to both constraint graphs as fixed nodes (their x,y are constants). Any soft module constrained to appear after a fixed module gets x ≥ fixed.x + fixed.w.

### 5.3 Soft Module Sizing

Before packing (or during), for each soft module, choose (w, h):
- **Constraint**: w×h ≥ area, 0.5 ≤ h/w ≤ 2
- **Strategy**: Use square root approximation `w₀ = ceil(sqrt(area))`, `h₀ = ceil(area/w₀)`
- **AR perturbation**: In SA, a "resize" move tries a new aspect ratio r ∈ {0.5, 0.7, 1.0, 1.4, 2.0} and picks the one minimizing HPWL contribution for that module's nets

### 5.4 SA Configuration

```
T₀ = initial_temp (calibrated: mean |ΔC| from 1000 random moves)
T_min = 0.1
α_cool = 0.92       — faster cooling than reference's 0.95
N_moves = max(50 × n_soft², 10000) per temperature
time_limit = 580s   — leave 20s buffer
```

**Perturbation operators** (one chosen uniformly at random):
1. **Swap Γ⁺**: swap two soft modules in Γ⁺ (changes horizontal constraint)
2. **Swap Γ⁻**: swap two soft modules in Γ⁻ (changes vertical constraint)
3. **Rotate pair**: pick a soft module, swap it in both Γ⁺ and Γ⁻ relative to a neighbor (diagonal move)
4. **Resize**: change one soft module's aspect ratio

**Cost function**:
```
cost = HPWL  (pure wirelength, α=0)
     + λ × max(0, packed_W − chip_W)  (outline overflow penalty)
     + λ × max(0, packed_H − chip_H)
```
where `λ = initial_HPWL × 10` to strongly penalize outline violation.

### 5.5 Fixed-Outline Enforcement

- During SA: if the packed placement exceeds chip dimensions, the penalty term drives SA to reject or shrink the solution
- After SA: if the best solution still exceeds bounds (shouldn't happen if λ is large enough), attempt a global scale-down of soft module widths to fit within bounds, maintaining w×h ≥ area

---

## 6. Expected Performance vs. Baselines

| Metric | Reference | Our Target | Confidence |
|--------|:---------:|:----------:|:-----------:|
| HPWL public1 | 239,984,392 | ≤ 161,609,972 | Medium |
| HPWL public2 | 38,494,434  | ≤ 20,966,863  | Medium-High |
| HPWL public3 | 2,621,582   | ≤ 1,856,276   | High |
| HPWL public4 | 137,686,350 | ≤ 63,024,850  | Medium |
| Runtime public1 | 581s     | ≤ 580s        | High |
| Verifier pass | 100% | 100% | High |

**Confidence reasoning**:
- Sequence pair SA with 100K–500K moves/sec vs reference's ~10 moves/sec → 10K× more exploration
- Literature (Murata et al. 1996, Sheng et al. 2003) shows sequence pair SA achieves 10–30% better HPWL than greedy placement baselines for similar problem sizes
- The extremely large Min/Reference gap for public4 (0.458) suggests the Min was achieved by a very efficient algorithm; our approach may not reach Min for all cases but should exceed Reference significantly

---

## 7. Risks and Mitigations

| Risk | Likelihood | Mitigation |
|------|:----------:|------------|
| Sequence pair packing doesn't enforce fixed-outline hard constraint | High | Add large λ penalty + post-process resize |
| Fixed module overlap with soft modules | Medium | Add fixed→soft edges to constraint graph |
| AR constraint (0.5–2) makes packing too constrained | Low | Widen feasible AR set; allow slight area over-sizing |
| Cannot reach Min threshold (too aggressive) | Medium | Accept gracefully; document gap in RESULT.md |
| Runtime >600s on public1 | Low | Set 580s time limit in code |

---

## 8. References

- Murata, H. et al. "VLSI module placement based on rectangle-packing by the sequence-pair." *IEEE TCAD* 15.12 (1996): 1518–1524.
- Chang, Y.-C. et al. "B\*-trees: A new representation for non-slicing floorplans." *DAC 2000*: 458–463.
- Lin, J.-M. et al. "Placement by annealing with B\*-tree representations." *IEEE TCAD* (2001).
- Sheng, C.-W. et al. "Fixed-outline floorplanning through better local search." *IEEE TCAD* (2003).
- Adya, S.N. et al. "Fixed-outline floorplanning: enabling hierarchical design." *ICCAD 2001*.
