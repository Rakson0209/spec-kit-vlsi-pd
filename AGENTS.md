# AGENTS.md - spec-kit-vlsi-pd

This repo uses GitHub Spec Kit / Specify for VLSI physical design automation experiments.

## Codex Spec Kit Invocation

Codex uses project skills in `.agents/skills`. Invoke these as `$speckit-*`, not Claude-style `/speckit.*` slash commands.

Core workflow:

```text
$speckit-specify -> $speckit-plan -> $speckit-tasks -> $speckit-implement
```

Optional workflow skills:

```text
$speckit-clarify
$speckit-checklist
$speckit-analyze
$speckit-constitution
$speckit-taskstoissues
$speckit-agent-context-update
```

## Project Rules

Constitution: `.specify/memory/constitution.md`.

The project structure is:

```text
problems/<NNN-*>/
  reference/      baseline/source/spec/report
  benchmark/      testcases and verifier
  experiments/    per-model generated specs and implementations
```

Phase 1 creates shared specs under `problems/<NNN-*>/experiments/claude-opus-4-8/spec/`.
Phase 2 implementations may copy that spec into `problems/<NNN-*>/experiments/<model>/spec/` and run `$speckit-implement` with `SPECIFY_FEATURE_DIRECTORY=...`.

Build command baseline:

```powershell
g++ -std=c++20 -O3 -fopenmp -pthread -o <exe> main.cpp
```

Score command:

```powershell
python scorer/score.py <problem-num> --output-dir <out-dir> --label <model>
```

Important constraints:

- R0: Do not rely on hidden subagents for independent implementation work.
- R1: Baseline fallback is allowed only when self-written code cannot beat scorer baselines; reference object files may not be portable on Windows.
- R2: Prefer portable C++20 with OpenMP/pthread; avoid unnecessary external dependencies.
- R3: Verify with benchmark scorer/testcases before reporting completion.

See `CLAUDE.md` for the existing detailed project memory and active plan notes.
