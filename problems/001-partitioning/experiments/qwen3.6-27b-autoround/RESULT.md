# RESULT - 001-partitioning qwen3.6-27b-autoround

## Summary
All 7 testcases produce **valid** output (scorer OK).
**5/6** public cases beat the reference solution.
R1 Baseline Fallback: **NOT triggered** (only triggers if ALL cases worse than reference).

## Results vs Reference

| testcase | Ours | Reference | Status | Improvement |
|---|---|---|---|---|
| sample | 1 | - | ✅ | - |
| public1 | **173** | 193 | ✅ | -10.4% |
| public2 | **3473** | 3666 | ✅ | -5.3% |
| public3 | **3709** | 7092 | ✅ | -47.7% |
| public4 | **1686** | 2265 | ✅ | -25.6% |
| public5 | **888** | 1669 | ✅ | -46.8% |
| public6 | 60409 | 10281 | ❌ | +487.4% |

## Runtime

| testcase | Wall Time |
|---|---|
| sample | 0.00s |
| public1 | 0.06s |
| public2 | 5.57s |
| public3 | 2.76s |
| public4 | 0.32s |
| public5 | 0.86s |
| public6 | 10.86s |

## Approach
- **Algorithm**: Fiduccia-Mattheyses (FM) local search with rollback-to-best-prefix
- **Initialization**: 4 diverse strategies (greedy, area-descending, random, balanced-alternating)
- **Parallel**: OpenMP multi-start with dynamic scheduling
- **Per-case tuning**:
  - NC < 5000: 256 starts, 5s budget, unrestricted FM
  - NC < 50000: 256 starts, 15s budget, unrestricted FM
  - NC < 200000: 32 starts, 60s budget, cumG-limited FM
  - NC > 200000: 32 starts, 285s budget, cumG-limited FM
- **Build**: g++ -std=c++20 -O3 -fopenmp -pthread (no Boost)

## public6 Analysis
public6 (740K cells, 758K nets, maxDeg=531) significantly underperforms vs reference (60409 vs 10281).
The reference runs ~225s with a single FM optimization; our 32 parallel starts each get ~10s of FM passes.
The large maxDeg (531) makes gain bucket management expensive, limiting FM pass throughput.

Potential improvements:
- Multilevel coarsening (T020-T022) to reduce problem size before FM
- Sequential refinement of best result after parallel multi-start
- Adaptive per-start time estimation based on actual FM pass duration

## Files
- Solver: `main.cpp` (518 lines)
- Build: `Makefile`
- Output: `out/*.out`
- Scores: `score.md`, `score.csv`
