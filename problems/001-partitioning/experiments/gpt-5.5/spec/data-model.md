# Phase 1 Data Model: Multi-Technology Die Partitioning

**Spec**: [spec.md](./spec.md) · **Plan**: [plan.md](./plan.md) · **Research**: [research.md](./research.md)

Two layers: **(A) input-domain entities** (from the spec) and **(B) internal solver structures** (from research §6–7). Field types are guidance, not prescription.

---

## A. Input-domain entities

### Technology (Tech)
- `name: string` — e.g. `TA`, `TB`.
- `libArea: map<libCellName → double>` — area (`width × height`) of each LibCell **under this Tech**.
- There are `NumTechs` of these; exactly two are referenced (one per die).

### LibCell
- `name: string` — e.g. `MC1`.
- Dimensions `(width, height)` are **per-Tech**; the same LibCell name appears in every Tech with possibly different dimensions. The solver only needs the derived **area per Tech**.

### Die (DieA, DieB)
- `tech: string` — the Tech this die uses.
- `cap: double` — utilization cap as a fraction (`util% / 100`).
- `area: double` — physical area = `DieSize.W × DieSize.H` (**shared** by both dies).
- **Derived state**: `usedArea` (Σ area of assigned cells, in this die's Tech). **Invariant (FR-004)**: `usedArea / area ≤ cap + 1e-9`.

### Cell
- `name: string` — unique; e.g. `C1`.
- `libCell: string` — the LibCell it instantiates.
- **Assignment**: exactly one of `{A, B}` (FR-003 — full, exclusive coverage).
- **Derived**: `areaA = Tech[DieA.tech].libArea[libCell]`, `areaB = Tech[DieB.tech].libArea[libCell]` (side-dependent weight).

### Net
- `name: string`; `degree: int` — member count.
- `members: list<cellName>` — incident cells (a cell may, per the scorer, appear more than once; only distinct sides matter).
- **Cut contribution (the metric)**: `1` iff members touch both A and B, else `0`.

**Entity relationships**

```
Tech 1──*  (libArea entries)        Die *──1 Tech        Die 1──* Cell (assignment)
Cell *──* Net  (pins / membership)  Cell *──1 LibCell    Net  1──* Cell (members)
```

---

## B. Internal solver structures (research §6–7)

> Name interning: map each cell name → contiguous `int id ∈ [0, NumCells)`; keep `id→name` for output. Same for nets if helpful.

### Per-cell arrays (Structure-of-Arrays)
| field | type | meaning |
|-------|------|---------|
| `areaA[i]`, `areaB[i]` | `double` | side-dependent area (precomputed) |
| `side[i]` | `uint8` (0=A,1=B) | current assignment |
| `locked[i]` | `bool` | locked this FM pass |
| `gain[i]` | `int` | current FM gain |
| `cellPins` | CSR `values[] + offset[]` | net ids incident to each cell |

### Per-net arrays
| field | type | meaning |
|-------|------|---------|
| `countA[n]`, `countB[n]` | `int` | #incident cells on each side (FM `F/T`) |
| `netPins` | CSR `values[] + offset[]` | cell ids incident to each net |

### Die accumulators
- `usedArea[A]`, `usedArea[B]`: `double`; updated incrementally on each move.

### Gain buckets (research §6)
- `bucket[g + maxDeg]` → doubly-linked list of unlocked cell ids with gain `g`, `g ∈ [−maxDeg, +maxDeg]`.
- `maxGainPtr`: current highest non-empty bucket.

### Multilevel hierarchy (large cases, research §5)
- Stack of levels; each level: coarsened cell/net arrays + `clusterOf[]` mapping fine→coarse. Cluster area accumulates per Tech.

---

## State transition — one FM move (cell `i`, A→B)

**Precondition**: `!locked[i]` and `usedArea[B] + areaB[i] ≤ capB·area + 1e-9` (feasibility, FR-004).

**Effect**:
1. `usedArea[A] -= areaA[i]`; `usedArea[B] += areaB[i]`.
2. `side[i] = B`; `locked[i] = true`; remove `i` from its bucket.
3. For each net `n` in `cellPins[i]`: decrement `countA[n]`, increment `countB[n]`; for neighbor cells whose `F/T` crossed `0` or `1`, recompute their gain delta and move them between buckets.
4. Update cumulative gain; if a new prefix maximum, record the move index.

**Pass end**: roll back all moves after the recorded best-prefix index (restoring `side`, `usedArea`, counts), so the pass is monotone non-worsening.

---

## Validation rules (← spec FRs; enforced by scorer)

| Rule | Source | Where enforced |
|------|--------|----------------|
| Every cell assigned exactly once (no missing/dup/unknown) | FR-003 | init covers all; moves only flip side |
| `usedArea[d]/area ≤ cap[d] (+1e-9)` for both dies | FR-004 | feasibility gate on every move + init |
| Cell area uses the **die's Tech** dimensions | FR-005 | `areaA/areaB` precompute |
| Output counts equal listed names; name = first token | FR-006 | writer |
| Self-reported `CutSize` == actual cut | FR-007/SC-006 | compute cut from final `side[]` before writing |
| Objective = #nets touching both sides | FR-008 | `countA[n]>0 && countB[n]>0` |
