# Contract: Output File Format (`.floorplan`)

**Used by**: Output writer (Step 5 of implementation)
**Consumed by**: `benchmark/verifier/verify` and `scorer/score.py`

---

## Format

```
Wirelength <hpwl>
                        ← blank line
NumSoftModules <n>
<name> <x> <y> <w> <h>
...                     ← n lines, one per soft module
```

## Field Types

| Field | Type | Constraint |
|-------|------|------------|
| hpwl | integer | self-reported HPWL (scorer recomputes; this is informational) |
| n | integer | must equal NumSoftModules from input |
| name | string | must match a SoftModule name from input |
| x, y | non-negative integer | bottom-left corner of placed module |
| w, h | positive integer | final dimensions of soft module |

## Legality Requirements (checked by verifier and scorer)

1. `x >= 0`, `y >= 0`, `x + w <= chip_W`, `y + h <= chip_H`
2. `w * h >= min_area` (from input)
3. `0.5 <= h/w <= 2.0`
4. No overlap with any other module (soft or fixed)
5. All soft modules from input must appear exactly once

## HPWL Computation (as verified by scorer)

```
cx = x + w // 2    (integer floor division)
cy = y + h // 2
HPWL = Σ_{net(A,B,weight)} weight × (|cx_A − cx_B| + |cy_A − cy_B|)
```

Fixed modules: use their input `(x, y, w, h)` for center calculation.
Fixed modules do NOT appear in the output — only soft modules.

## Example (from sample.txt solution)

```
Wirelength 275

NumSoftModules 2
GPU 0 0 5 5
CPU 5 0 3 5
```
