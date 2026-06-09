# CLI Contract: Global Placement tool

**Plan**: [../plan.md](../plan.md) | **Spec**: [../spec.md](../spec.md)

## Invocation

```
<tool> <input.aux> <output.gp.pl>
```

Exactly **two** positional arguments (matches `reference/src/main.cpp` and how the scorer launches a `--run` binary). No flags required.

| Arg | Meaning |
|-----|---------|
| `argv[1]` | path to the Bookshelf `.aux` (entry that lists `.nodes .nets .wts .pl .scl`) |
| `argv[2]` | path to write the result `.gp.pl` |

- Wrong arg count → print usage to `stderr`, exit non-zero. (Reference prints usage and `return 0`; we exit non-zero so the scorer flags misuse — but with correct args, success is exit 0.)
- **Success exit code: `0`** (FR-011/FR-014). Any non-zero exit is treated by the scorer as a failed run.

## Behavioral contract

| Guarantee | Requirement |
|-----------|-------------|
| Reads all five Bookshelf files reached from `.aux` | FR-002 |
| Writes a coordinate for **every movable** cell, exactly once | FR-003 / FR-008 |
| Never relocates a fixed cell / terminal | FR-005 |
| All movable lower-left bboxes inside core (within `1e-6`) | FR-004 |
| Output placement is **spread** (scorer Tetris avg disp ≤ `0.05×min(coreW,coreH)`, no abort) | FR-006 / SC-006 |
| Minimizes unweighted HPWL (scorer definition) | FR-007 |
| Completes each case ≤ ~590s wall-clock, exit 0 | FR-011 / SC-005 |
| Deterministic legality across reruns | FR-013 |

## Stdout / stderr

- Progress logging (benchmark name, core bounds, per-round HPWL/time) may go to `stdout`; it does **not** affect scoring (scorer reads only the `.gp.pl` file).
- If a `Current HPWL:` value is printed, it MUST equal the scorer's recomputation (pin-offset HPWL, research §3) — never an internal surrogate value.

## Build contract (constitution R2)

```
g++ -std=c++20 -O3 -fopenmp -pthread -o hw4 main.cpp
```

- Single self-contained translation unit; **no link against `reference/obj/*.o`** (those are Linux ELF — unlinkable on Windows, research §10).
- Windows-native exec constraint: place the built `.exe` under `D:\FSecret\` to run (project policy); output files may be written anywhere.
