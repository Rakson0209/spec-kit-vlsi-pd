# Quickstart: Global Placement — build, run, score

**Plan**: [plan.md](./plan.md) | **Contracts**: [cli](./contracts/cli-contract.md) · [io](./contracts/io-format.md)

End-to-end validation that the tool produces a **legal, low-HPWL** placement on all 3 public cases. Pure-Python scorer = single source of truth (R6).

## Prerequisites

- Portable toolchain loaded (PowerShell):
  ```powershell
  . .\tools\mingw64\setup-env.ps1
  # or reload machine+user PATH if g++ not found:
  $env:PATH = [Environment]::GetEnvironmentVariable("PATH","Machine")+";"+[Environment]::GetEnvironmentVariable("PATH","User")
  ```
- Python 3 for the scorer (no extra packages; cross-platform).
- **Windows exec policy**: a self-compiled `.exe` only runs from `D:\FSecret\`. Build/copy the binary there to run it; output `.gp.pl` files may live anywhere.

## 1. Build (constitution R2)

```powershell
$model = "claude-opus-4-8"
$src   = "problems/003-global-placement/experiments/$model/main.cpp"
g++ -std=c++20 -O3 -fopenmp -pthread -o D:\FSecret\hw4.exe $src
```
No link against `reference/obj/*.o` (Linux ELF — unlinkable on Windows; research §10). Single self-contained source.

## 2. Run all 3 public cases

```powershell
$tc  = "problems/003-global-placement/benchmark/testcase"
$out = "problems/003-global-placement/experiments/$model/out"
New-Item -ItemType Directory -Force $out | Out-Null
foreach ($c in "public1","public2","public3") {
    D:\FSecret\hw4.exe "$tc/$c/$c.aux" "$out/$c.gp.pl"
}
```
Each case must exit 0 within ~590s (the largest, `public3`, is the time-critical one).

## 3. Score (legality + HPWL, all cases — R3/R4)

```powershell
python scorer/score.py 003 --output-dir $out --label $model
```

The scorer matches each `<case>.gp.pl` to its `.aux`, recomputes HPWL, runs the legality + anti-collapse health check, and prints a table.

## Expected outcomes (acceptance)

| Gate | Pass condition | Maps to |
|------|----------------|---------|
| **Legality** | every case `valid=OK` (no missing / out-of-core / moved-fixed / collapse) | SC-001, FR-003/04/05/06 |
| **Spread margin** | `note` shows `avgDisp` ≤ `0.05×core` (ideally ≈0.01×) | SC-006 |
| **Scoring** | HPWL strictly `< Max` every case | SC-002 |
| **Competitive** | HPWL `≤ Reference` every case, `<` on ≥1 case | SC-004 (R1 keep self-written) |
| **Headline** | HPWL `≤ Min` every case | SC-003 |
| **Runtime** | each case ≤ ~590s, exit 0 | SC-005 |

Target table (lower = better):

| testcase | Min (≤ goal) | Reference | Max (zero) |
|----------|-------------:|----------:|-----------:|
| public1  |   59,788,412 |  87,987,694 |    319,198,465 |
| public2  |   10,530,075 |  18,642,174 |     28,999,635 |
| public3  |  395,131,978 | 750,902,922 |  2,631,834,205 |

## 4. Iterate (R1 / R4)

1. Score **all 3** cases after every change; watch `valid` **and** the `avgDisp` note (early-warning for the collapse check).
2. If HPWL is worse than **Reference** on **all** cases → fall back to the reference *algorithm* (LSE WL + bell density + λ ramp), re-implemented in our self-contained code, then re-tune (research §10). Keep self-written code if it beats Reference on **any** case.
3. Record results in `experiments/$model/RESULT.md` (template in `experiments/README.md`): per-case `valid`, HPWL, runtime, rounds, notes.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| `valid=NG`, note `legalize=aborted`/large `avgDisp` | placement collapsed / under-spread | raise density weight λ / finer bins / more ramp rounds (research §4) |
| `out_of_core` violation | center not clamped before output | clamp lower-left bbox to `[xmin,xmax]×[ymin,ymax]` (research §7) |
| `moved fixed` violation | wrote terminal with changed coords | skip fixed in output, or emit exact input coords (FR-005) |
| `missing` violation | a movable cell absent from output | emit every movable cell once (FR-003) |
| timeout on `public3` | iteration budget too high | wall-clock guard ~560s, keep best-so-far (research §9) |
| HPWL far above Min but legal | bins too coarse / too few iters / random init | adaptive bins, WL-aware init, spend full budget (research §4/§5/§9) |
| `g++ not found` / exe `Access denied` | PATH not reloaded / wrong dir | reload PATH; build into `D:\FSecret\` |
