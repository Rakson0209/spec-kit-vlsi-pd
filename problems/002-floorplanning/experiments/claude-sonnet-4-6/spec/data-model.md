# Data Model: Fixed-outline Floorplanning

**Feature**: Fixed-outline Floorplanning Optimizer
**Date**: 2026-06-05
**References**: [spec.md](spec.md), [plan.md](plan.md), [research.md](research.md)

---

## Core Entities

### Module

Represents a single block (soft or fixed) on the chip.

| Field | Type | Description |
|-------|------|-------------|
| `id` | int | Zero-based index (soft modules first, then fixed) |
| `name` | string | Unique identifier from input file |
| `x` | int | Left edge x-coordinate (output coordinate) |
| `y` | int | Bottom edge y-coordinate (output coordinate) |
| `w` | int | Width (mutable for soft; fixed for fixed modules) |
| `h` | int | Height (mutable for soft; fixed for fixed modules) |
| `min_area` | int | Minimum required area (`w×h ≥ min_area` must hold) |
| `is_fixed` | bool | True = FixedModule (position/dimensions immutable) |

**Invariants** (soft modules only):
- `w × h >= min_area`
- `0.5 ≤ h/w ≤ 2.0`
- `x >= 0`, `y >= 0`, `x + w <= chip_W`, `y + h <= chip_H`

**Invariants** (fixed modules):
- `x`, `y`, `w`, `h` match input exactly; never modified

**Center point** (used in HPWL): `(cx, cy) = (x + w/2, y + h/2)` — integer floor division

---

### Net

Represents a weighted 2-pin connection between two modules.

| Field | Type | Description |
|-------|------|-------------|
| `module_a` | int (index) | First module ID |
| `module_b` | int (index) | Second module ID |
| `weight` | int | Connection weight (positive integer) |

**HPWL contribution**: `weight × (|cx_a − cx_b| + |cy_a − cy_b|)`

---

### Chip

The fixed bounding rectangle for the entire placement.

| Field | Type | Description |
|-------|------|-------------|
| `W` | int | Chip width |
| `H` | int | Chip height |

**Constraint**: All module rectangles must satisfy `0 ≤ x`, `0 ≤ y`, `x+w ≤ W`, `y+h ≤ H`.

---

### SequencePair

Encodes relative positions of soft modules as two permutations.

| Field | Type | Description |
|-------|------|-------------|
| `pos_x` | vector<int> | Γ⁺: permutation of soft module indices |
| `pos_y` | vector<int> | Γ⁻: permutation of soft module indices |

**Semantics**: Module `a` is to the left of module `b` iff `a` appears before `b` in both Γ⁺ and Γ⁻. Module `a` is below module `b` iff `a` appears before `b` in Γ⁺ but after `b` in Γ⁻.

---

### FloorplanState

The complete mutable state at any SA step.

| Field | Type | Description |
|-------|------|-------------|
| `sp` | SequencePair | Current sequence pair (soft modules only) |
| `modules` | vector<Module> | Current module positions and dimensions |
| `hpwl` | long long | Current HPWL value |
| `overflow_W` | int | `max(0, packed_W − chip_W)` |
| `overflow_H` | int | `max(0, packed_H − chip_H)` |
| `cost` | double | SA cost = HPWL + λ × (overflow_W + overflow_H) |

---

### AspectRatioSet

Pre-computed set of valid (w, h) pairs for each soft module.

| Field | Type | Description |
|-------|------|-------------|
| `shapes` | vector<pair<int,int>> | Valid (w, h) pairs sampled from AR range |

**Sampling rule**: For each soft module with min_area `A`:
- `w_min = ceil(sqrt(A/2))`, `w_max = floor(sqrt(2A))`
- Sample ~20 values of `w` in `[w_min, w_max]` with `h = ceil(A/w)`
- All samples satisfy `w×h ≥ A` and `0.5 ≤ h/w ≤ 2`

---

## State Transitions

### SA Move Types

| Move | Modifies | Effect |
|------|----------|--------|
| `swap_gp` | `sp.pos_x` | Swap two random soft modules in Γ⁺ |
| `swap_gm` | `sp.pos_y` | Swap two random soft modules in Γ⁻ |
| `rotate_pair` | `sp.pos_x`, `sp.pos_y` | Move module to random position in both sequences |
| `resize_ar` | `modules[i].w`, `.h` | Change soft module aspect ratio from pre-computed set |

After any move: repack (rebuild constraint graphs → longest paths → new x,y), recalculate HPWL and cost.

### Best Solution Tracking

- Best solution = minimum `hpwl` among all states where `overflow_W == 0` and `overflow_H == 0`
- If no legal solution found, track minimum-cost infeasible solution for debugging

---

## Data Flow

```
Input .txt
    ↓ Parser
Chip, vector<Module>, vector<Net>
    ↓ Initialize
SequencePair (random)  →  Pack  →  FloorplanState
    ↓ SA loop
   Perturb → Pack → Evaluate cost → Accept/Reject
    ↓ best_state
Output .floorplan (soft modules only)
```

---

## Name Lookup

During parsing and output, a `name → module_id` map is maintained:

```
unordered_map<string, int> name_to_id
```

Used to resolve net endpoints from their string names to integer indices.
