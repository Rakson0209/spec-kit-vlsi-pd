# Quickstart & Validation: Fixed-Outline Floorplanning

End-to-end validation guide. Build → run → score → compare to targets. Implementation details live in [plan.md](./plan.md) / [research.md](./research.md) / `tasks.md`.

## Prerequisites

- PowerShell on Windows.
- Toolchain: `g++` 16.1.0 — load via `. .\tools\mingw64\setup-env.ps1`, **or** rely on the winget WinLibs g++ on User PATH. Because each PowerShell invocation is a fresh process, reload PATH first if needed:
  ```powershell
  $env:PATH = [Environment]::GetEnvironmentVariable("PATH","Machine") + ";" + [Environment]::GetEnvironmentVariable("PATH","User")
  ```
- Python 3 on PATH (for the scorer; pure-Python, cross-platform).
- Testcases in `problems/002-floorplanning/benchmark/testcase/` (`sample.txt`, `public1..4.txt`).
- **Exec constraint** (Windows ASR/EDR): an unsigned self-compiled `.exe` only runs from **`D:\FSecret\`**. Build the binary there (data files may live anywhere).

## Build (constitution R2 — OpenMP + pthread, no Boost)

```powershell
$src = "problems\002-floorplanning\experiments\<model>\main.cpp"
$exe = "D:\FSecret\hw3.exe"          # MUST be under D:\FSecret to execute
g++ -std=c++20 -O3 -fopenmp -pthread -o $exe $src
```

## Run on the sample (US1 smoke test)

```powershell
$out = "problems\002-floorplanning\experiments\<model>\out"
New-Item -ItemType Directory -Force $out | Out-Null
& $exe "problems\002-floorplanning\benchmark\testcase\sample.txt" "$out\sample.floorplan"
```

**Expected**: exit code 0; `out\sample.floorplan` begins with an optional `Wirelength <n>` line, then `NumSoftModules 2`, then exactly two lines `GPU x y w h` and `CPU x y w h` — each in the `8×7` outline, `w·h ≥ 25/15`, ratio in `[0.5,2]`, and not overlapping `PAD1 (0,5,2,2)` / `FIXED1 (5,0,3,2)`.

## Score one case

```powershell
python scorer\score.py 002 --output-dir "$out" --cases sample --label <model>
```

**Expected**: `valid=True` (the scorer prints `wirelength=… valid=True` and an `OK` row).

## Score ALL testcases (constitution R4 gate)

```powershell
# 1) produce outputs for every case (run the exe from D:\FSecret)
foreach ($c in "sample","public1","public2","public3","public4") {
  & $exe "problems\002-floorplanning\benchmark\testcase\$c.txt" "$out\$c.floorplan"
}
# 2) batch-score + write reports
python scorer\score.py 002 --output-dir "$out" --label <model> `
  --md "problems\002-floorplanning\experiments\<model>\score.md" `
  --csv "problems\002-floorplanning\experiments\<model>\score.csv"
```

Alternatively let the scorer drive the binary with `--run "D:\FSecret\hw3.exe"` (it executes the exe per case; subprocess timeout 1200s).

## Success criteria mapping (← spec)

| Gate | Check | Spec |
|------|-------|------|
| **Legality** | every row `valid=OK` (5/5) | SC-001 / US1 |
| **Scores at all** | each public `wirelength` < Max | SC-002 |
| **Beats baseline (R1)** | `wirelength` ≤ Reference on all, `<` on ≥1 | SC-004 |
| **Headline target** | `wirelength` ≤ Min | SC-003 / US2 |
| **Runtime** | each case ≤ ~600s | SC-005 / US3 |
| **Self-report** | `Wirelength` line == scorer metric | SC-006 |

Targets (lower better) — `Min ≤ goal`, `Reference` = R1 bar, `Max` = zero-score:

| case | Min | Reference | Max |
|------|----:|----------:|----:|
| public1 | 161,609,972 | 239,984,392 | 349,768,634 |
| public2 | 20,966,863  | 38,494,434  | 41,569,628  |
| public3 | 1,856,276   | 2,621,582   | 5,045,921   |
| public4 | 63,024,850  | 137,686,350 | 201,625,050 |

## R1 Baseline Fallback (constitution, research §11)

If, after one iteration of self-written code, **all** cases are worse than Reference: copy `reference/src/main.cpp` into the model dir, build with the **same flags** (the reference is **Boost-free** — no porting needed, just `g++ -std=c++20 -O3 -fopenmp -pthread`), then optimize (drop its `alpha·area` cost term → pure wirelength; add parallel multi-start). If **any** case already beats Reference, keep the self-written code.

## Troubleshooting

- `Access is denied` running the exe → it is not under `D:\FSecret\`; rebuild the output there. (Claude's sandbox may also need `dangerouslyDisableSandbox: true` to execute it.)
- `valid=NG` overlap → two rectangles intersect; check the strict-inequality `overlap` test and that fixed modules are pre-marked as obstacles.
- `valid=NG` out-of-outline → enforce `x+w ≤ W && y+h ≤ H` on every move and at init.
- `valid=NG` aspect/area → a soft shape violates `w·h ≥ area` or `h/w ∈ [0.5,2]`; verify every candidate shape (research §4).
- self-reported `Wirelength` ≠ score → use integer **floor** centers `(x+w//2, y+h//2)` exactly (research §10).
- over ~600s or DNF on `public1` → enforce the wall-clock deadline `< 600s` and emit best-so-far (research §9); ensure the `H×W` grid was dropped for rectangle math.
