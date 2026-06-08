# Data Model: Global Placement

**Date**: 2026-06-08 | **Plan**: [plan.md](./plan.md) | **Research**: [research.md](./research.md)

In-memory structures for a single self-contained C++ translation unit. Designed for **flat, index-based** access (no name lookups in hot loops) and **cache-friendly** SoA layout, sized for ~51k cells / ~50k nets / ~188k pins. All geometry in `double`; identities by contiguous integer id.

---

## Entities

### Cell (module)
Index `0 … N−1`, assigned in `.nodes` order.

| Field | Type | Source | Notes |
|-------|------|--------|-------|
| `name` | string (id→name table) | `.nodes` | only for output; not used in hot loops |
| `w, h` | double | `.nodes` | cell size; movable cells have `h == rowHeight` |
| `fixed` | bool | `.nodes` `terminal` **OR** `.pl` `/FIXED` | immovable anchor (FR-005) |
| `cx, cy` | double | optimization variable | **center** position (analytical var); fixed cells hold input center |
| `x, y` (derived) | double | `cx−w/2, cy−h/2` | lower-left, for in-core test & output |

- **Movable list** `mov[]`: indices with `fixed==false`. Optimization variable vector `X` has length `2·|mov|` (`X[2k], X[2k+1]` = center of `mov[k]`); fixed cells are constants in the objective.
- **Validation**: every movable cell present in output exactly once (FR-003); `xmin ≤ cx−w/2`, `cx+w/2 ≤ xmax`, same in y after clamp (FR-004); fixed `cx,cy` ≡ input (FR-005).

### Net
Index `0 … M−1`, in `.nets` order. CSR (compressed) layout:

| Field | Type | Notes |
|-------|------|-------|
| `net_ptr[M+1]` | int[] | CSR offsets into pin arrays |
| `pin_cell[P]` | int[] | cell index of each pin (flattened) |
| `pin_xoff[P], pin_yoff[P]` | double[] | pin offset relative to cell lower-left (from `.nets`) |
| `weight` | double (optional) | from `.wts`; **not applied** to scored HPWL (research §3) |

- A net's pins are `pin_cell[net_ptr[i] … net_ptr[i+1])`.
- **Pin global coord** = `(cell.x + xoff, cell.y + yoff) = (cx − w/2 + xoff, cy − h/2 + yoff)`.
- **HPWL(net)** = `(max−min) pin_x + (max−min) pin_y`, summed over nets (unweighted).
- **Validation**: degenerate nets (degree ≤ 1 or repeated cell) contribute a well-defined value, never crash (FR edge cases).

### Core / Rows
From `.scl` `CoreRow` blocks (research §7, [bookshelf.parse_core](../../../../../scorer/lib/bookshelf.py) / [legalize.parse_rows](../../../../../scorer/lib/legalize.py)):

| Field | Type | Notes |
|-------|------|-------|
| `xmin, ymin, xmax, ymax` | double | core bounds; **may be negative** |
| `rows[]` = `{y, h, x0, x1, sp}` | struct[] | per-subrow: bottom y, height, left/right x, site spacing |
| `rowHeight` | double | common row height (== movable cell height) |

- `xmin = min x0`, `ymin = min y`, `xmax = max x1`, `ymax = max(y+h)`.
- Used for the in-core clamp (FR-004) and optional site/row snapping (research §7).

### Bin grid (density)
Derived, rebuilt per λ-round; **adaptive resolution** (research §4).

| Field | Type | Notes |
|-------|------|-------|
| `binCutX, binCutY` | int | adaptive, e.g. `≈ √(coreArea/(k·avgCellArea))`; not fixed 14 |
| `Wb, Hb` | double | bin width/height = core span / binCut |
| `targetDensity` | double | `Σ movable area / coreArea` |
| `binDensity[binCutX·binCutY]` | double[] | accumulated bell-smoothed area; rebuilt each FG eval |

### Optimizer state
| Field | Type | Notes |
|-------|------|-------|
| `X[2·|mov|]` | double[] | current centers (variables) |
| `grad[2·|mov|]` | double[] | objective gradient (WL + λ·density) |
| `gamma (γ)` | double | WL smoothing; start ≈ coreW/10, annealed |
| `lambda (λ)` | double | density penalty weight; ramped 0 → up |
| CG work vectors | double[] | direction / previous-gradient (own CG, research §6/§10) |

- **Thread-local** `grad`/`binDensity` partials reduced in fixed order for determinism (research §6).

---

## Relationships

```
Placement
├── Cell[N]         (movable: optimization vars; fixed: constants/anchors)
├── Net[M] (CSR)    ──< Pin[P] >── Cell      (pin → cell, with offset)
├── Core/Rows       (clamp bounds + legalizability structure)
└── BinGrid         (density penalty over Cell centers)

Objective f(X) = WL(X; γ)  +  λ · Σ_bins (binDensity − targetDensity)²
grad   = ∇WL  +  λ · ∇density           (both parallel-reduced)
```

## Lifecycle / state transitions

1. **Parse** `.aux → .nodes/.nets/.pl/.scl/.wts` → fill Cell/Net(CSR)/Core/Rows; mark fixed; set fixed centers from `.pl`.
2. **Init** (research §5): movable centers ← core center; short λ=0 WL warmup.
3. **Ramp loop** (research §4/§9): for each round — set γ, set λ (ramp), CG-minimize `f`, clamp movable centers in-core; check time guard (~560s) and spread convergence (estimated legalize disp under `0.05×`).
4. **Finalize**: clamp; optional site/row snap if it lowers scorer disp; keep best-so-far.
5. **Output** (research §8): write movable cells `name x y : N` (lower-left) to `.gp.pl`.

## Invariants (map to requirements)

- Fixed cells never written with changed coords / never enter `X` (FR-005).
- Every movable cell emitted once, lower-left bbox in-core within `1e-6` (FR-003/FR-004).
- Final global placement is spread: scorer Tetris avg disp `≤ 0.05 × min(coreW,coreH)`, no abort (FR-006/SC-006).
- Self-reported HPWL (if printed) recomputed with pin offsets == scorer's value (research §3/§7).
- Zero dependency on `reference/obj` (research §10).
