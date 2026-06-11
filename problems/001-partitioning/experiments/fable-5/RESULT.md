# RESULT — 001-partitioning / fable-5

**Status**: ✅ done — **7/7 valid=OK**, beats Reference on **all 6** public cases, **≤ Min on 4/6**.

## Scores (scorer = `scorer/lib/partitioning.py`, label `fable-5`)

| testcase | cut (fable-5) | Min (≤ goal) | Reference | Max | vs Ref | vs Min | runtime |
|----------|--------------:|-------------:|----------:|----:|-------:|-------:|--------:|
| sample   | 1     | —     | —      | —       | —     | —     | 0.02s |
| public1  | 100   | 104   | 193    | 1,441   | 0.52× | **≤ Min** ✓ | 0.4s |
| public2  | 773   | 816   | 3,666  | 27,862  | 0.21× | **≤ Min** ✓ | 80s |
| public3  | 1,172 | 1,762 | 7,092  | 103,659 | 0.17× | **≤ Min** ✓ | 170s |
| public4  | 863   | 982   | 2,265  | 12,421  | 0.38× | **≤ Min** ✓ | 26s |
| public5  | 653   | 297   | 1,669  | 48,964  | 0.39× | 2.20× | 31s |
| public6  | 5,468 | 5,159 | 10,281 | 490,120 | 0.53× | 1.06× | 183s |

Reports: [score.md](./score.md) · [score.csv](./score.csv)

## Success-criteria mapping (spec)

- **SC-001 legality**: 7/7 `valid=OK` ✓
- **SC-002 below Max**: all publics ≪ Max ✓
- **SC-003 ≤ Min**: 4/6 (public1/2/3/4) ✓; public5 2.2× Min, public6 1.06× Min
- **SC-004 ≤ Reference everywhere, < on ≥1**: all 6 strictly below Reference (0.17×–0.53×) ✓
- **SC-005 runtime ≤ ~300s**: max 183s (public6); deterministic run counts sized to stay inside the ~285s internal deadline ✓
- **SC-006 self-report == scorer**: equal on every case ✓

## R1 Baseline Fallback decision (T026)

**Keep self-written.** Every public case is strictly better than `reference/src/` (R1 needs only one); no porting of the Boost-based reference was required.

## Determinism (T027, FR-013)

Re-runs of public1 (cut 100) and public5 (cut 653) reproduce identical cuts and legality.
All randomness is seeded from restart/run indices; parallel multi-start reduces by (cut, index);
run/V-cycle/ILS counts are fixed functions of input size (the ~282s wall-clock deadline is an
emergency stop only — never reached, worst case 183s).

## Approach (single `main.cpp`, ~800 lines, no Boost)

1. **Parse**: whole-file buffered read + manual tokenizer; cell names interned to int ids
   (`string_view` into the file buffer); per-cell `areaA/areaB` precomputed from the two dies' techs.
2. **Finest hypergraph**: CSR both directions; per-net pin dedup, deg<2 nets dropped, **identical
   nets merged with weight** (cut = weighted spanning sum == scorer's count).
3. **FM engine**: weighted bucket-list gains (O(1) extract/update), destination-cap feasibility
   gate `used + area ≤ cap·W·H` (matches scorer's ≤ + 1e-9 semantics with margin), exact incremental
   cut tracking, roll-back-to-best-prefix passes, stall early-exit; nets with deg ≥ 3000 are counted
   but skipped for gain terms.
4. **Init**: feasibility-first greedy (relative-cost, plus shuffled / noisy / balance-aware variants
   per seed) + best-effort repair.
5. **Multilevel** (n > 12k): first-choice heavy-edge coarsening (net-weight/(deg−1) scoring,
   cluster-area cap, deg>64 nets skipped in scoring) to ~4k clusters → **OpenMP parallel multi-start
   FM** (24 restarts) + **ILS** (perturb-with-flips-and-swaps + FM, keep-if-better) at the coarsest
   level → uncoarsen with per-level FM refinement.
6. **V-cycles**: re-coarsen constrained to the current partition (only same-side merges), refine at
   every level; per independent run until 3 consecutive non-improvements, then final V-cycles on the
   global best. Independent run count fixed by pins (32 / 20 / 12).
7. **Flat path** (n ≤ 12k): 64–128 parallel multi-start FM + 8 parallel ILS chains.
8. **Safety**: ~282s wall-clock deadline → always emits the best legal partition found; final
   legality re-verified before writing; output = `CutSize` + `DieA/DieB` lists per contract.

Parallelism (R2): OpenMP `parallel for` over multi-start restarts and ILS chains; build flags
`-std=c++20 -O3 -fopenmp -pthread -static` (static link so the scorer subprocess needs no MinGW DLLs).

## Key empirical findings

- **V-cycles + ILS are the big lever**: first-cut multilevel gave public5 2066 / public6 10433
  (both *above* Reference); adding partition-constrained V-cycles, coarse-level ILS, swap
  perturbations, and a balance-aware init variant brought them to 653 / 5468.
- **Swap perturbations matter under tight caps** (public5: both dies ~79% of an 80% cap — single
  moves are mostly infeasible, pairwise A↔B exchanges keep ILS effective).
- **Run-to-run variance is large** on public3/5 (best 1172 vs worst 3732 across seeds), so many
  independent seeded runs + keep-best is essential; counts kept fixed for determinism.
