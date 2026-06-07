# Quickstart & Validation: Multi-Technology Die Partitioning

End-to-end validation guide. Build → run → score → compare to targets. Implementation details live in [plan.md](./plan.md) / [research.md](./research.md) / `tasks.md`.

## Prerequisites

- PowerShell on Windows.
- Portable toolchain loaded: `. .\tools\mingw64\setup-env.ps1` (provides `g++` 16.1.0).
- Python 3 on PATH (for the scorer; pure-Python, cross-platform).
- Testcases present in `problems/001-partitioning/benchmark/testcase/` (`sample.txt`, `public1..6.txt`; large ones per [benchmark/README.md](../../../benchmark/README.md)).

## Build (constitution R2 — no Boost)

```powershell
. .\tools\mingw64\setup-env.ps1
$d = "problems\001-partitioning\experiments\<model>"
g++ -std=c++20 -O3 -fopenmp -pthread -o "$d\hw2.exe" "$d\main.cpp"
```

## Run on the sample (US1 smoke test)

```powershell
& "$d\hw2.exe" `
  "problems\001-partitioning\benchmark\testcase\sample.txt" `
  "$d\out\sample.out"
```

**Expected**: exit code 0; `out\sample.out` begins with `CutSize <n>`, then `DieA <countA>` + names, `DieB <countB>` + names; all 8 cells (`C1..C8`) appear exactly once.

## Score one case

```powershell
python scorer\score.py 001 `
  --output-dir "$d\out" --cases sample --label <model>
```

**Expected**: `valid=True` (the scorer prints `cut_size=… valid=True` and an `OK` row).

## Score ALL testcases (constitution R4 gate)

```powershell
# 1) produce outputs for every case
foreach ($c in "sample","public1","public2","public3","public4","public5","public6") {
  & "$d\hw2.exe" "problems\001-partitioning\benchmark\testcase\$c.txt" "$d\out\$c.out"
}
# 2) batch-score
python scorer\score.py 001 --output-dir "$d\out" --label <model> `
  --md "$d\score.md" --csv "$d\score.csv"
```

Alternatively let the scorer drive the binary with `--run "$d\hw2.exe"` (it executes the exe per case, subprocess timeout 1200s).

## Success criteria mapping (← spec)

| Gate | Check | Spec |
|------|-------|------|
| **Legality** | every row `valid=OK` (7/7) | SC-001 / US1 |
| **Scores at all** | each public `cut_size` < Max | SC-002 |
| **Beats baseline (R1)** | `cut_size` ≤ Reference on all, `<` on ≥1 | SC-004 |
| **Headline target** | `cut_size` ≤ Min | SC-003 / US2 |
| **Runtime** | each case ≤ ~300s | SC-005 / US3 |
| **Self-report** | `CutSize` line == scorer metric | SC-006 |

Targets (lower better) — `Min ≤ goal`, `Reference` = R1 bar, `Max` = zero-score:

| case | Min | Reference | Max |
|------|----:|----------:|----:|
| public1 | 104 | 193 | 1,441 |
| public2 | 816 | 3,666 | 27,862 |
| public3 | 1,762 | 7,092 | 103,659 |
| public4 | 982 | 2,265 | 12,421 |
| public5 | 297 | 1,669 | 48,964 |
| public6 | 5,159 | 10,281 | 490,120 |

## R1 Baseline Fallback (constitution, research §10)

If, after one iteration of self-written code, **all** cases are worse than Reference: copy `reference/src/main.cpp` into `$d`, port it off Boost (`boost::unordered_map`→`std::unordered_map`; drop `boost/fusion`; add `<unordered_map>`), build with the same flags (no `-I boost`), then optimize. If **any** case already beats Reference, keep the self-written code.

## Troubleshooting

- `valid=NG` with utilization violation → a die exceeds its cap; tighten the move feasibility gate (`≤ cap + 1e-9`).
- `valid=NG` with coverage violation → missing/duplicate/unknown cell name in output; verify counts equal listed lines and names are interned/emitted once.
- DNF on `public6` → enforce the ~285s wall-clock deadline and emit best-so-far (research §8).
