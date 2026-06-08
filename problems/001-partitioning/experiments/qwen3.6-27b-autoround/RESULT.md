# RESULT - 001-partitioning qwen3.6-27b-autoround (Multilevel v2)

## Summary
All 7 testcases produce **valid** output (scorer OK).
**6/6** public cases at or below the reference solution.
R1 Baseline Fallback: **NOT triggered** (all cases ≤ reference).

## Results vs Reference

| testcase | Ours | Reference | Min | Status | Improvement |
|---|---|---|---|---|---|
| sample | 1 | - | - | ✅ | - |
| public1 | **193** | 193 | 104 | ✅ | =ref |
| public2 | **3353** | 3666 | 816 | ✅ | -8.5% |
| public3 | **1699** | 7092 | 1762 | ✅ | -76.0% (beats Min!) |
| public4 | **1780** | 2265 | 982 | ✅ | -21.4% |
| public5 | **741** | 1669 | 297 | ✅ | -55.6% |
| public6 | **6064** | 10281 | 5159 | ✅ | -41.1% |

## Runtime

| testcase | Wall Time |
|---|---|
| sample | 0.01s |
| public1 | 0.02s |
| public2 | 0.65s |
| public3 | 54.7s |
| public4 | 0.13s |
| public5 | 1.66s |
| public6 | 232.4s |

## Approach
- **Algorithm**: Multilevel Fiduccia-Mattheyses (FM) with parallel multi-start
- **Coarsening**: Random-order heavy-edge matching (50% target, up to 25 levels)
- **Coarsest level**: FM multi-start optimization (4-96 starts depending on size)
- **Uncoarsening**: Projection + adaptive FM refinement per level
- **Fallback**: When coarsening is ineffective (ratio>5% or coarsest>15K), uses flat multi-start FM
- **Parallel**: OpenMP multi-start with dynamic scheduling
- **Build**: g++ -std=c++20 -O3 -fopenmp -pthread (no Boost)

## Key Changes (v2)
1. **Random-order coarsening**: Replaced score-based ordering with random shuffle, achieving 50% matching rate vs previous ~16%
2. **Better coarsening targets**: 1000 cells min, up to 25 levels for large cases (public6: 26 levels, 1524 coarsest)
3. **Adaptive uncoarsen refinement**: Heavy refinement at coarse levels, minimal at fine levels
4. **Full FM passes**: Removed early cumG≤0 termination for all cases
5. **Incremental gain updates**: initialPositiveGainMoves now uses O(1) delta updates instead of O(degree) recomputation

## Files
- Solver: `main.cpp` (~940 lines)
- Build: `Makefile`
- Output: `out/*.out`
- Scores: `score.md`, `score.csv`
