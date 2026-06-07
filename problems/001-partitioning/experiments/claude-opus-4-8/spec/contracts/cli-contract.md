# Contract: Command-Line Interface

**Spec**: [../spec.md](../spec.md) (FR-001, FR-010, FR-012) · Consumer: [`scorer/score.py`](../../../../../../scorer/score.py) `run_impl`

## Invocation

```
<executable> <input_path> <output_path>
```

- The scorer launches the tool as `subprocess.run([exe, input_path, output_path], timeout=1200)`.
- `argc == 3`; `argv[1]` = input `.txt` path (read), `argv[2]` = output `.out` path (write/overwrite).
- **No other arguments, env vars, stdin, or interactive prompts.** Working directory must not matter (use the given paths verbatim).

## Exit status

| Condition | Exit code | Scorer interpretation |
|-----------|-----------|-----------------------|
| Success (output written) | `0` | proceeds to score the output |
| Any failure | non-zero | `run_impl` reports `非零退出 <code>` → case fails |

## Output side effects

- MUST create/overwrite exactly `argv[2]` with a well-formed result (see [io-format.md](./io-format.md)). Parent directory is created by the scorer when using `--run`; do not assume otherwise when run standalone — create it if missing.
- `stdout`/`stderr` are **not** parsed by the scorer for partitioning; optional progress/timing logs may go to `stderr`. Do not print the result to stdout in lieu of the file.

## Timing

- Hard subprocess timeout: **1200s** (scorer). Project budget: **~300s** per case (README). Internal deadline target: **~285s** (research §8) — on expiry, write the best legal partition found and exit `0`.

## Determinism (FR-013)

- Re-running on the same input MUST yield an equal-or-better cut and identical legality. Seed any randomness from a fixed function of restart index (not wall-clock).
