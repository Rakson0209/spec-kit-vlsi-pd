# Phase 1 Data Model: Fixed-Outline Floorplanning

**Spec**: [spec.md](./spec.md) · **Plan**: [plan.md](./plan.md) · **Research**: [research.md](./research.md)

Two layers: **(A) input-domain entities** (from the spec) and **(B) internal solver structures** (from research §3–8). Field types are guidance, not prescription. **All coordinates and dimensions are integers** (the scorer parses with integer conversion and floors centers).

---

## A. Input-domain entities

### Chip outline
- `W: int`, `H: int` — the fixed placement region `[0,W] × [0,H]`, anchored at origin.
- **Invariant (FR-004)**: every module rectangle satisfies `0 ≤ x`, `0 ≤ y`, `x+w ≤ W`, `y+h ≤ H`.

### SoftModule
- `name: string` — unique; e.g. `GPU`.
- `area: int` — **minimum** area (lower bound; realized `w·h` may exceed it).
- **Chosen by solver**: `x, y: int` (position), `w, h: int` (shape).
- **Derived**: `center = (x + w/2, y + h/2)` with **integer floor** division.
- **Invariants**: `w·h ≥ area` (FR-005); `0.5 ≤ h/w ≤ 2.0` within `1e-9` (FR-006); in-outline (FR-004); non-overlapping with all other modules (FR-007).
- **Output**: emitted exactly once as `name x y w h` (FR-003/FR-009).

### FixedModule
- `name: string` — unique; e.g. `PAD1`.
- `x, y, w, h: int` — **immovable**, taken verbatim from input (FR-008).
- **Derived**: `center = (x + w/2, y + h/2)` — a **constant** (precompute once).
- Acts as an **obstacle** (non-overlap) and a **net anchor** (wirelength). **Never written to output.**

### Net
- Two endpoints `A, B: moduleName` (each may be a soft *or* a fixed module) and `weight: int`.
- Every net here is exactly **2-pin** (grammar `Net A B weight`).
- **Wirelength contribution (the metric)**: `weight · (|cAx − cBx| + |cAy − cBy|)` using floored centers.

**Entity relationships**

```
ChipOutline 1──* SoftModule        ChipOutline 1──* FixedModule (as given)
Net 2── {SoftModule | FixedModule}  (endpoints A,B)        weight: int
center(module) = (x + w//2, y + h//2)   // floor; fixed centers are constant
```

---

## B. Internal solver structures (research §3–8)

> Name interning: map each module name → contiguous `int id`; keep `id→name` for output. A single id space over soft+fixed lets nets reference either uniformly; a `isFixed[id]`/`isSoft[id]` flag distinguishes them.

### Module record (Structure-of-Arrays or one struct/array)
| field | type | meaning |
|-------|------|---------|
| `x[i], y[i]` | `int` | lower-left position |
| `w[i], h[i]` | `int` | current shape (fixed: constant) |
| `fixed[i]` | `bool` | true → immovable obstacle/anchor |
| `area[i]` | `int` | soft: minimum area; fixed: `w·h` |
| `shapes[i]` | `vector<pair<int,int>>` | soft: legal candidate `(w,h)` set (research §4) |
| `cx[i], cy[i]` | `int` | cached center `(x+w//2, y+h//2)`; recompute on move/reshape |
| `incident[i]` | `vector<int>` (CSR-ish) | net ids touching module `i` |

### Net record
| field | type | meaning |
|-------|------|---------|
| `a[n], b[n]` | `int` | endpoint module ids |
| `wt[n]` | `int` | net weight |

### Solver state
- `totalWL: long long` — running weighted HPWL (sum over nets); 64-bit (values reach ~10⁸–10⁹, products of weight×span can be large → use `long long`).
- `best: {x,y,w,h per soft module, WL}` — best **legal** solution found (per chain, and global after reduction).
- `rng` — per chain, seeded by chain index (determinism, research §9).
- `deadline` — wall-clock cutoff `< 600s` (research §9).

### Geometry primitives (research §3, grid-free)
- `overlap(i,j)`: `x[i] < x[j]+w[j] && x[j] < x[i]+w[i] && y[i] < y[j]+h[j] && y[j] < y[i]+h[i]` — **strict** (edge-touch legal). Exactly the scorer's `_overlap`.
- `inOutline(i)`: `x[i]≥0 && y[i]≥0 && x[i]+w[i]≤W && y[i]+h[i]≤H`.
- `legalPlacement(i)`: `inOutline(i)` and no `overlap(i,j)` for any `j≠i`.
- `compactDown/Left(i)`: slide to the lowest/leftmost blocked position via `O(n)` rectangle maxima (no grid).

---

## State transition — one solver move (soft module `i`)

**Move kinds**: `translate` (new `x,y`), `reshape` (new `(w,h)` from `shapes[i]`), `swap(i,j)` (exchange positions, reshape to fit), `nudgeToMedian` (move center toward weighted median of neighbors).

**Precondition**: `!fixed[i]`; the proposed `(x,y,w,h)` must give `legalPlacement(i)` (in-outline, area `w·h ≥ area[i]`, ratio `h/w ∈ [0.5,2]`, no overlap).

**Effect**:
1. Recompute `cx[i], cy[i]` from the new `(x,y,w,h)`.
2. For each net `n ∈ incident[i]`: `delta += wt[n]·(newSpan − oldSpan)` where `span = |cAx−cBx| + |cAy−cBy|`.
3. `totalWL += delta`.
4. **Accept** if `delta ≤ 0` (descent) or Metropolis (SA: `rand() < exp(−delta/T)`); else **revert** `(x,y,w,h,cx,cy)` and undo `totalWL`.
5. If accepted and `totalWL < best.WL`, snapshot `best`.

**Reduction (after parallel chains)**: pick the chain whose `best.WL` is smallest; tie-break by lowest chain index → deterministic global best.

---

## Validation rules (← spec FRs; enforced by scorer)

| Rule | Source | Where enforced |
|------|--------|----------------|
| Every soft module output exactly once (no missing/dup) | FR-003 | init places all; writer emits all |
| Soft module fully in outline `x+w≤W, y+h≤H, x,y≥0` | FR-004 | `inOutline` gate on every move + init |
| `w·h ≥ area` (area is a lower bound) | FR-005 | shape generation guarantees it; checked per move |
| `0.5 ≤ h/w ≤ 2.0` (±`1e-9`) | FR-006 | candidate shapes verified (research §4) |
| No overlap among soft+fixed (strict inequality) | FR-007 | `overlap` gate on every move + init; fixed pre-marked |
| Fixed modules unmoved & excluded from output | FR-008 | `fixed[i]` immutable; writer skips fixed |
| Output format `Wirelength?` / `NumSoftModules n` / `name x y w h` | FR-009 | writer |
| Self-reported `Wirelength` == scorer recompute | FR-010/SC-006 | compute `totalWL` with floor centers before writing |
| Objective = Σ `weight·(|Δcx|+|Δcy|)`, floor centers | FR-011 | `totalWL` definition |
