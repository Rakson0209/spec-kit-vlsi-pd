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

### 階段 0：Baseline 先行驗證（必須先做）

在開始任何演算法優化前，先複製 `reference/src/` 的參考解，確認所有 testcase 都能通過：

```powershell
# 複製 reference 程式碼到實驗目錄
Copy-Item -Recurse reference/src/* experiments/<model>/

# 編譯
. .\tools\mingw64\setup-env.ps1
g++ -std=c++20 -O3 -o hw3 main.cpp

# 對所有 testcase 執行並用 scorer 驗證
# 全部合法（OK）後再開始優化，避免卡在單一 case
```

### 階段 1：編譯與執行

**前置**：先執行 `tools\mingw64\setup-env.bat`（或 `. .\tools\mingw64\setup-env.ps1`），
將內建 portable g++ 加入 `PATH`。此編譯器已隨專案打包，解壓縮後無需安裝。

```sh
# 從專案根目錄
# Step 0: 設定 portable C++ 環境（PowerShell）
. .\tools\mingw64\setup-env.ps1

# Step 1: 編譯（From experiments/<model>/）
g++ -std=c++20 -O3 -o hw3 main.cpp

# Step 2: 執行
./hw3 ../../benchmark/testcase/sample.txt out_sample.floorplan

# 加速編譯選項（全部內建支援）
# OpenMP 平行化：g++ -std=c++20 -O3 -fopenmp -o hw3 main.cpp
# 多執行緒：   g++ -std=c++20 -O3 -pthread -o hw3 main.cpp
# 全開：       g++ -std=c++20 -O3 -fopenmp -pthread -o hw3 main.cpp
# Boost（需先下載至 tools/boost/）：g++ -std=c++20 -O3 -I tools/boost -o hw3 main.cpp

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

## Spec-Kit SDD Workflow (CodeWhale Skills)

This project uses [GitHub spec-kit](https://github.com/github/spec-kit) for
Spec-Driven Development. The workflow is implemented as CodeWhale skills in
`.claude/skills/`. Trigger any skill by mentioning its name or the slash
command:

| Step | Skill | Slash Command | Artifacts |
|------|-------|---------------|-----------|
| ① Spec | `speckit-specify` | `/speckit.specify` | `spec/spec.md` + checklist |
| ② Clarify | `speckit-clarify` | `/speckit.clarify` | Updated spec |
| ③ Plan + Research | `speckit-plan` | `/speckit.plan` | `research.md`, `plan.md`, `data-model.md`, `contracts/`, `quickstart.md` |
| ④ Analyze | `speckit-analyze` | `/speckit.analyze` | Consistency report |
| ⑤ Tasks | `speckit-tasks` | `/speckit.tasks` | `tasks.md` |
| ⑥ Implement | `speckit-implement` | `/speckit.implement` | Source code + scorer verification |
| 🏛️ Constitution | `speckit-constitution` | `/speckit.constitution` | Updated `.specify/memory/constitution.md` |

### How to use

Start a new experiment by saying:
```
/speckit.specify SPECIFY_FEATURE_DIRECTORY=problems/<NNN-problem>/experiments/<model>/spec
<problem description>
```

Or trigger any skill by name: "use speckit-plan to create the implementation plan".

### Two gates (Constitution Principles VI & VII)

- **Research gate**: `speckit-plan` must produce `research.md` with algorithm survey
  and selection to beat Min thresholds.
- **Legality gate**: `speckit-implement` must verify ALL testcases pass scorer
  legality check before declaring completion.

### Scorer verification

```powershell
python scorer/score.py <problem-num> --output-dir <out-dir> --label <model> --md <report.md>
```

### Skill files

- `.claude/skills/speckit-specify/SKILL.md`
- `.claude/skills/speckit-plan/SKILL.md`
- `.claude/skills/speckit-tasks/SKILL.md`
- `.claude/skills/speckit-implement/SKILL.md`
- `.claude/skills/speckit-clarify/SKILL.md`
- `.claude/skills/speckit-constitution/SKILL.md`
- `.claude/skills/speckit-analyze/SKILL.md`