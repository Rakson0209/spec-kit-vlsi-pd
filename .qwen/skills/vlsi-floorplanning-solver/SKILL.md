---
name: vlsi-floorplanning-solver
description: C++11 fixed-outline floorplanning solver using grid-less SA with force-directed init and corner-based legalization
source: auto-skill
extracted_at: '2026-06-05T08:01:15.506Z'
---

# VLSI Floorplanning Solver Implementation

## Overview

Implements a fixed-outline floorplanning solver in C++11 that reads `.txt` input (chip size, soft/fixed modules, nets) and produces `.floorplan` output minimizing weighted HPWL while satisfying legality constraints (no overlap, within outline, area/aspect ratio).

## Algorithm Pipeline

```
parse_input() → precompute_shapes() → force_init() → legalize() → sa_loop() → write_output()
```

### 1. Parse Input (`parse()`)

- Read `ChipSize W H`, `SoftModule name area`, `FixedModule name x y w h`, `Net A B weight`
- Build `Module[]` array (fixed modules first, then soft)
- Build `Net[]` array with module index references
- Map module names to indices via `name2idx`

### 2. Precompute Shapes (`precompute_shapes()`)

For each soft module, enumerate all valid `(w, h)` pairs:
- `w` ranges from `ceil(sqrt(area/2))` to `ceil(sqrt(2*area))*3` (capped at `chip_w`)
- `h = ceil(area / w)`
- Filter: `w*h >= area`, `h <= chip_h`, `0.5 <= h/w <= 2.0`
- Keep up to 20 shapes per module

### 3. Force-Directed Initialization (`force_init()`)

1. Assign initial square-ish shape (closest to AR=1 from precomputed list)
2. Center all soft modules at `(chip_w/4, chip_h/4)`
3. Run 100 iterations of force-directed relaxation:
   - For each net, compute spring force between module centers proportional to weight
   - Move each soft module by `min(step, |force|/5)` in force direction
   - `step` starts at `max(W,H)*0.25`, decays by 0.95 each iteration
   - Clamp positions to stay within chip outline

### 4. Corner-Based Legalization (`legalize()`)

1. Record target centers from force-init
2. Sort soft modules by connectivity (most nets first)
3. Start with corner candidates from fixed modules (`x, x+w, y, y+h`)
4. For each soft module:
   - Try each shape (sorted by closeness to AR=1)
   - For each corner candidate, try offsets `[-w..w]` and `[-h..h]` with step `max(1, dim/2)`
   - Collect up to 200 valid placements, pick closest to target center
   - Add placed module's corners to candidate pool
5. Fallback: coarse grid scan with step `max(W/H, 1)/5`
6. Last resort: force at origin with first shape

### 5. Simulated Annealing (`sa_loop()`)

**Temperature Calibration**:
- 2000 random perturbations to measure `|ΔHPWL|` distribution
- `T0 = mean(|ΔHPWL|) * 0.5`, minimum 5000

**Operators** (selected by uniform random 0-99):
- `op < 40`: **Relocate** — compute weighted centroid of neighbors, search corner candidates with 3 offset variants per corner
- `40 <= op < 60`: **Shape change** — pick random shape, try current position then shuffled corner candidates
- `60 <= op < 80`: **Translate** — random ±offset (range = max(50, max(W,H)/20))
- `80 <= op`: **Swap** — swap positions of two random soft modules, optionally with shape change

**SA Inner Loop**:
- Run until `uphill_count >= 3*num_soft` or `total_generations >= 300*num_soft` or `T < 0.5` or time limit
- Accept if `Δ < 0` or `random < exp(-Δ/T)`
- Decay `T *= 0.97` (first round) or `0.985` (reheat rounds)

**Reheat-Intensify**:
- Up to 5 rounds; after first round, restore best solution
- Reheat `T = T0 * 0.3`, faster decay `0.985`
- Stop early if no improvement in a reheat round

### 6. Output (`write_out()`)

Format per scorer specification:
```
Wirelength <integer>

NumSoftModules <n>
<name> <x> <y> <w> <h>
...
```

## Key Data Structures

```cpp
struct Shape { int w, h; };

struct Module {
    string name;
    int x, y, w, h, area;
    bool fixed;
    vector<Shape> shapes;
    vector<int> adj;          // net indices involving this module
    int bk_x, bk_y, bk_w, bk_h;  // backup for SA
    int cx() const { return x + w / 2; }  // integer floor division
    int cy() const { return y + h / 2; }
};

struct Net { int a, b, w; };  // module indices + weight
```

## Legality Constraints

- **Outline**: `0 <= x`, `0 <= y`, `x+w <= chip_w`, `y+h <= chip_h`
- **Area**: `w*h >= min_area` (from input)
- **Aspect ratio**: `0.5 <= h/w <= 2.0`
- **No overlap**: `ax < bx+bw && bx < ax+aw && ay < by+bh && by < ay+ah` (all pairs)
- **Fixed modules**: position immutable (not in output, read from input by scorer)

## HPWL Calculation

```
HPWL = Σ weight_i * (|cx_a - cx_b| + |cy_a - cy_b|)
```

Centers use integer floor division: `cx = x + w/2`.

## Performance Considerations

- **No grid**: Avoids O(chip_area) memory (critical for public1 with 117M cells)
- **Corner candidate limiting**: Shuffle and limit to 60 candidates per dimension in relocate
- **Full HPWL per step**: Simpler than incremental, acceptable for N <= 42 modules
- **Time limits**: Auto-scale by module count (10s for <=5, 180s for <=20, 580s for larger)

## Build

```bash
g++ -std=c++11 -O3 -Wall -Wextra -o hw3 main.cpp -lm
```

## Usage

```bash
./hw3 <input.txt> <output.floorplan>
```

## Scoring

```bash
python scorer/score.py 002 --output-dir <output_dir> --label <name>
```

## Reproducibility

- Fixed RNG seed: `mt19937 rng(42)`
- Time tracking: `chrono::steady_clock`
- Runtime limit with early exit to stay under 600s total
