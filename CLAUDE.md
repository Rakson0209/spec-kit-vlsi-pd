# CLAUDE.md — spec-kit-vlsi-pd

This file provides coding agent context for the spec-kit-vlsi-pd project.

**Current Model**: `deepseek-v4-flash` (DeepSeek V4 Flash)

## Project Overview

Spec-Driven Development (SDD) framework for VLSI physical design problems.
Each LLM model independently runs the full SDD chain (spec → plan → tasks → implement)
from the **same problem statement** (`reference/spec.pdf` + `benchmark/`) and produces its
**own spec**; results are compared objectively. The shared baseline is the problem statement
and evaluation assets, not the generated spec.

**Constitution**: `.specify/memory/constitution.md`

## Active Feature

<!-- SPECKIT START -->
**Feature**: 002 Fixed-outline Floorplanning — `deepseek-v4-flash`
**Spec**: `problems/002-floorplanning/experiments/deepseek-v4-flash/spec/spec.md`
**Plan**: `problems/002-floorplanning/experiments/deepseek-v4-flash/spec/plan.md`
**Algorithm**: B\*-tree + Simulated Annealing with adaptive soft-module sizing
**Target**: HPWL ≤ Min thresholds (161.6M / 21.0M / 1.86M / 63.0M for public1–4)
**Status**: sample: ✅ OK (215); public1: HPWL 153M ✅ ≤ Min 161M but ❌ overlap with PAD modules
<!-- SPECKIT END -->

## Key Directories

```
problems/002-floorplanning/
├── benchmark/
│   ├── testcase/   # Input .txt files (sample + public1–4)
│   └── verifier/   # Official verifier binary (Linux)
├── experiments/    # Per-model full SDD chain + implementation
│   └── <model>/    # e.g., claude-sonnet-4-6/
│       ├── spec/   # This model's own SDD artifacts (spec, plan, research, data-model, tasks)
│       └── ...     # This model's implementation + eval data
└── reference/      # Baseline impl + spec.pdf (the shared problem statement)
scorer/             # Cross-platform Python scorer (Windows-compatible)
```

## Build & Run

```sh
# From experiments/<model>/
g++ -std=c++11 -O3 -o hw3 main.cpp
./hw3 ../../benchmark/testcase/sample.txt out_sample.floorplan

# Verify (Linux)
../../benchmark/verifier/verify ../../benchmark/testcase/sample.txt out_sample.floorplan

# Score (Windows)
python scorer/score.py problems/002-floorplanning/benchmark/testcase/sample.txt \
       problems/002-floorplanning/experiments/<model>/out_sample.floorplan
```

## Core Principles (Constitution Summary)

1. **Spec-First**: spec.md → plan.md → tasks.md → code. No implementation without spec.
2. **Verifier as Truth**: `benchmark/verifier/verify` is the only correctness judge.
3. **Fair Comparison**: each model's full SDD chain (own `spec/`) + code lives in `experiments/<model>/`, no cross-model sharing. Shared baseline = problem statement + benchmark + scorer + env (not the spec).
4. **Reproducible**: record model, compile cmd, run cmd, testcase, verifier result, HPWL, time.
5. **Quantify Quality**: report HPWL numbers, compare against baseline.
6. **Research-First, Beat the Baseline**: before any code, `research.md` must record the baseline metric as the bar, survey candidate algorithms, and pick one with credible potential to **surpass** the baseline (not just approach it). No tasks/implement until research is done.
