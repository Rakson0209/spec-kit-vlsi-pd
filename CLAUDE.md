# CLAUDE.md — spec-kit-vlsi-pd

## 專案說明

以 SDD 流程實作 VLSI 實體設計題目，比較不同 LLM 模型產出。
Constitution: `.specify/memory/constitution.md`

## 核心規則（最重要）

**R1 Baseline Fallback**：先自寫 code → 跑 scorer → 若**全部** case 都比 `reference/src/` 差 → 複製 reference 並在其上優化。只要有任何 case 優於 reference，就繼續用自己的 code。

**R2 Boost + 平行優先**：編譯一律 `g++ -std=c++20 -O3 -fopenmp -pthread -I tools/boost`。優先使用 `boost::graph`、`boost::geometry`、OpenMP 平行化。

**R3 合法性 + 全 case**：所有 testcase scorer OK。每次改 code 後對所有 case 跑 scorer。

## 編譯

```powershell
. .\tools\mingw64\setup-env.ps1
g++ -std=c++20 -O3 -fopenmp -pthread -I tools/boost -o hw3 main.cpp
```

## 計分

```powershell
python scorer/score.py <problem-num> --output-dir <out-dir> --label <model>
```

## SDD 流程

`/speckit.specify` → `/speckit.plan` → `/speckit.tasks` → `/speckit.implement`

Skill 檔案：`.claude/skills/speckit-*/SKILL.md`

<!-- SPECKIT START -->
Current plan: problems/001-partitioning/experiments/claude-opus-4-8/spec/plan.md
<!-- SPECKIT END -->
