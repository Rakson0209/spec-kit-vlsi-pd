# 002 Fixed-Outline Floorplanning — Result (claude-opus-4-8, self-written)

**Build**: `g++ -std=c++20 -O3 -fopenmp -pthread -o hw3 main.cpp` (no Boost) ·
**Exec**: `D:\FSecret\hw3.exe <input.txt> <output.floorplan>` · **Machine**: 12 logical cores.

## Scores (scorer = `scorer/lib/floorplanning.py`, the sole arbiter, R6)

| case | wirelength | valid | self-report == score | runtime | Min | Reference | Max | WL/Min | WL/Ref |
|------|-----------:|:-----:|:---:|--------:|----:|----------:|----:|:------:|:------:|
| sample  | 215         | OK | ✓ |  14.3s | —           | —           | —           | —      | —      |
| public1 | 158,054,586 | OK | ✓ | 245.7s | 161,609,972 | 239,984,392 | 349,768,634 | **0.978×** ✅ | 0.659× |
| public2 | 24,136,170  | OK | ✓ | 304.0s | 20,966,863  | 38,494,434  | 41,569,628  | 1.151× | 0.627× |
| public3 | 1,918,931   | OK | ✓ | 321.6s | 1,856,276   | 2,621,582   | 5,045,921   | 1.034× | 0.732× |
| public4 | 62,396,925  | OK | ✓ | 285.5s | 63,024,850  | 137,686,350 | 201,625,050 | **0.990×** ✅ | 0.453× |

## Gate summary (← spec Success Criteria)

- **SC-001 legality** — 5/5 `valid=OK` (zero violations). ✅
- **SC-002 scores at all** — every public WL strictly below Max. ✅
- **SC-004 beats baseline (R1 bar)** — WL ≤ Reference on **all** public cases, strictly below on all four
  (0.45×–0.73× of Reference). ✅
- **SC-003 headline target (≤ Min)** — **public1 (0.978×) and public4 (0.990×) beat Min**; public3 within
  3.4%, public2 (the 93 %-dense case) within 15 %. ✅ on 2/4, close on the rest.
- **SC-005 runtime** — every case ≤ ~322s, well under the ~600s budget. ✅
- **SC-006 self-report** — written `Wirelength` equals the scorer's recompute on all cases. ✅

## R1 Baseline Fallback decision (constitution R1, research §11)

**KEEP the self-written solver — no fallback.** R1 triggers only if *every* case is worse than
`reference/src/`; here the self-written code beats Reference on **all four** public cases (and beats the
**Min** target on two). `reference/src/main.cpp` was never copied or modified.

## Why it beats the single-start, area-burdened reference

1. **Pure weighted-HPWL objective** — area/outline/aspect are constraints only, never in the cost (the
   reference spends half its budget on `α·area`, which the scorer ignores).
2. **Grid-free `O(n²)` rectangle geometry** — no `H×W` boolean grid (≈117M cells for public1), so every
   overlap/compaction op is cheap and millions of SA moves fit in budget.
3. **Weighted-median coordinate descent** — the analytical single-module L1 optimum, an intensifier the
   reference lacks.
4. **OpenMP parallel multi-start** — 72 diversified, seeded restarts reduced to the best legal result.

## Determinism (FR-015)

Stopping is **fixed-restart-budget** driven (72 restarts, seed = restart index), not time driven; every
case finishes in 245–322s — far under the 555s start-cutoff — so all 72 restarts complete and the result
is the deterministic min over a fixed seed set. The wall-clock deadline (585s) only truncates in a rare
overflow. Re-running yields identical wirelength and legality (T024 re-run confirmed).

## Reproduce

```powershell
. .\tools\mingw64\setup-env.ps1   # or reload PATH per quickstart.md
g++ -std=c++20 -O3 -fopenmp -pthread -o D:\FSecret\hw3.exe `
    problems\002-floorplanning\experiments\claude-opus-4-8\main.cpp
$out = "problems\002-floorplanning\experiments\claude-opus-4-8\out"
foreach ($c in "sample","public1","public2","public3","public4") {
  & D:\FSecret\hw3.exe "problems\002-floorplanning\benchmark\testcase\$c.txt" "$out\$c.floorplan"
}
python scorer\score.py 002 --output-dir $out --label claude-opus-4-8
```
