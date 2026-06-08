# Contract: Input / Output Format

**Source of truth**: `_parse_input` and `_parse_output` in [`scorer/lib/floorplanning.py`](../../../../../../scorer/lib/floorplanning.py). The scorer tokenizes with `read_token_lines` ([`scorer/lib/common.py`](../../../../../../scorer/lib/common.py)): **blank lines are skipped**, lines are split on **any whitespace**, and **extra trailing tokens on a line are ignored**. All numeric values are **integers**.

---

## Input grammar (`.txt`, read by the tool)

```
ChipSize <W> <H>
NumSoftModules <n>
SoftModule <name> <area>          ×n
NumFixedModules <m>
FixedModule <name> <x> <y> <w> <h>   ×m
NumNets <k>
Net <moduleA> <moduleB> <weight>     ×k
```

- Section keywords (`ChipSize`, `NumSoftModules`, `SoftModule`, …) are the first token of their line.
- `SoftModule`: `name` then **minimum** `area`.
- `FixedModule`: `name`, lower-left `x y`, size `w h` (immovable).
- `Net`: two module names (each soft or fixed) and an integer `weight`.
- Blank lines may separate sections (the `sample` and public cases do this) — skip them.

**Example** (`sample.txt`):

```
ChipSize 8 7

NumSoftModules 2
SoftModule GPU 25
SoftModule CPU 15

NumFixedModules 2
FixedModule PAD1 0 5 2 2
FixedModule FIXED1 5 0 3 2

NumNets 3
Net GPU CPU 20
Net GPU PAD1 10
Net CPU FIXED1 15
```

---

## Output grammar (`.floorplan`, written by the tool)

```
Wirelength <value>        # OPTIONAL self-report; scorer reads it case-insensitively
NumSoftModules <n>
<name> <x> <y> <w> <h>    ×n   # SOFT modules only; name is the FIRST token
```

- **Only soft modules** are listed — `n` MUST equal `NumSoftModules` from the input, and every soft module must appear **exactly once**. **Do not** emit fixed modules.
- Each module line: `name x y w h` (lower-left `x y`, chosen size `w h`). The scorer reads the first 5 tokens; anything after is ignored.
- The optional `Wirelength` line, if present, MUST equal the scorer's recomputed weighted HPWL for this floorplan (FR-010/SC-006). A blank line after it is fine (skipped).
- Reference writer for byte-level guidance: [`reference/src/main.cpp`](../../../../reference/src/main.cpp) lines ~500–510.

**Example** (a legal `sample.floorplan`; coordinates illustrative):

```
Wirelength 215

NumSoftModules 2
GPU 0 0 5 5
CPU 5 2 3 5
```

---

## Legality (checked by `score()` — all must hold for `valid=OK`)

| Check | Rule |
|-------|------|
| **Completeness** | every input soft module present in output |
| **In-outline** | `x ≥ 0 && y ≥ 0 && x+w ≤ W && y+h ≤ H` |
| **Min area** | `w·h ≥ area` (declared area is a lower bound) |
| **Aspect ratio** | `0.5 − 1e-9 ≤ h/w ≤ 2.0 + 1e-9` |
| **Non-overlap** | for every pair across **soft + fixed**: NOT(`ax < bx+bw && bx < ax+aw && ay < by+bh && by < ay+ah`) — shared edges (zero-area touch) are legal |
| **Net refs** | every net endpoint resolves to a known soft or fixed module |

## Metric (recomputed by `score()`)

```
center(module) = (x + w//2, y + h//2)          # integer FLOOR division
wirelength     = Σ over nets  weight · (|cAx − cBx| + |cAy − cBy|)
```

Fixed-module centers are constants from the input. Lower wirelength is better; per-case Min / Reference / Max targets are in [spec.md](../spec.md) (SC-003).
