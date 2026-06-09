# gpt-5.5 Floorplanning Result

Build:

```powershell
g++ -std=c++20 -O3 -fopenmp -pthread -o D:\FSecret\hw3.exe problems\002-floorplanning\experiments\gpt-5.5\main.cpp
```

Validation command:

```powershell
python scorer\score.py 002 --run D:\FSecret\hw3.exe --output-dir problems\002-floorplanning\experiments\gpt-5.5\out --label gpt-5.5 --md problems\002-floorplanning\experiments\gpt-5.5\score.md --csv problems\002-floorplanning\experiments\gpt-5.5\score.csv
```

| testcase | wirelength | valid | self-reported | Min | reference | decision |
|---|---:|:---:|---:|---:|---:|---|
| public1 | 175,276,050 | OK | 175,276,050 | 161,609,972 | 239,984,392 | beats reference |
| public2 | 26,308,875 | OK | 26,308,875 | 20,966,863 | 38,494,434 | beats reference |
| public3 | 2,029,090 | OK | 2,029,090 | 1,856,276 | 2,621,582 | beats reference |
| public4 | 62,979,325 | OK | 62,979,325 | 63,024,850 | 137,686,350 | beats Min |
| sample | 215 | OK | 215 | n/a | n/a | smoke OK |

R1 Baseline Fallback decision: keep the self-written solver. Every public testcase is legal and beats the reference, and `public4` also beats the published Min target, so `reference/src/` was not copied.

Additional checks:

- Existing output rescore after validation: 5/5 valid OK.
- Goal update: at least one testcase beats Min (`public4`: 62,979,325 < 63,024,850).
- Cleanup: no Boost include; binary builds with the required C++20/OpenMP/pthread flags.
