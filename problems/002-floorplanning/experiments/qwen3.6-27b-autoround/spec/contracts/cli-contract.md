# Contract: Command-Line Interface

**Consumer**: the project scorer (`scorer/score.py`), which launches the tool as a subprocess. **Source of truth**: `run_impl()` in [`scorer/score.py`](../../../../../../scorer/score.py).

## Invocation

```
hw3 <input.txt> <output.floorplan>
```

- **`argv[1]`** — path to an existing input testcase (`.txt`, grammar in [io-format.md](./io-format.md)).
- **`argv[2]`** — path to write the result (`.floorplan`). The scorer passes `<output-dir>/<case>.floorplan`.
- Exactly two positional arguments; no flags, no stdin, no env configuration.

The scorer invokes it as `subprocess.run([exe, input_path, output_path], timeout=1200)`.

## Behavioral requirements

| # | Requirement | Source |
|---|-------------|--------|
| 1 | Read `argv[1]`, write `argv[2]`; create the output's parent directory if missing. | FR-001 |
| 2 | **Exit code 0** on success. Any non-zero exit is treated as a failed run (the case scores nothing). | FR-014 |
| 3 | Finish within the runtime budget (~**600s**); the scorer kills the process at **1200s**. Keep an internal wall-clock deadline `< 600s` and always write a legal best-so-far before exiting. | FR-013 / SC-005 |
| 4 | Be deterministic: re-running on the same input yields equal-or-better wirelength and identical legality. | FR-015 |
| 5 | Write **only** to `argv[2]` (and create its parent dir). Do not depend on the current working directory. | FR-001 |

## Build / run (Windows, per [quickstart.md](./quickstart.md))

```
g++ -std=c++20 -O3 -fopenmp -pthread -o hw3 main.cpp
```

> **Exec constraint**: place the resulting `hw3.exe` under `D:\FSecret\` to run it (unsigned exes are blocked elsewhere by Defender ASR/EDR). Input/output data files may live anywhere.

## Non-requirements

- No stdout/stderr format is required (the scorer ignores them on success; on non-zero exit it captures the first 160 chars of stderr for the report).
- No support for malformed inputs beyond skipping blank lines / arbitrary whitespace (inputs are well-formed, spec Assumptions).
