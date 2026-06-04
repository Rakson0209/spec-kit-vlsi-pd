# QWEN.md — spec-kit-vlsi-pd

This file provides coding agent context for the spec-kit-vlsi-pd project (Qwen Code).

## Project Overview

Spec-Driven Development (SDD) framework for VLSI physical design problems.
Each LLM model independently runs the full SDD chain (spec → plan → tasks → implement)
from the **same problem statement** (`reference/spec.pdf` + `benchmark/`) and produces its
**own spec**; results are compared objectively. The shared baseline is the problem statement
and evaluation assets, not the generated spec.

**Constitution**: `.specify/memory/constitution.md`

## Active Feature

<!-- SPECKIT START -->
**Current Feature**: (none yet)
新的 SDD run 由 `/speckit.specify` 建立，產物置於 `problems/<NNN>/experiments/<model>/spec/`；
`/speckit.plan` 會自動把連結填入此區塊。
<!-- SPECKIT END -->

## Key Directories

```
problems/<NNN-題目>/
├── benchmark/
│   ├── testcase/   # 輸入測資
│   └── verifier/   # 官方驗證器（Linux）
├── experiments/    # 每模型的完整 SDD 鏈 + 實作
│   └── <model>/
│       ├── spec/   # 該模型自產的 SDD 產物（spec/plan/research/data-model/tasks）
│       └── ...     # 該模型的實作 + 評測數據（RESULT.md）
└── reference/      # 人類參考解 baseline + spec.pdf（共用題目敘述）
scorer/             # 跨平台 Python 計分器（Windows 原生可跑）
```

各題「Baseline 指標門檻（要超越的 Min 值）」記在該題 `README.md`。

## Core Principles (Constitution Summary)

1. **規格先行 (Spec-First)**：spec.md → plan.md → tasks.md → code，不得跳過。
2. **驗證器即真理**：`benchmark/verifier/verify` 是唯一正確性依據。
3. **公平比較**：每模型完整 SDD 鏈（自有 `spec/`）+ code 放 `experiments/<model>/`，不跨模型共用；共同基準 = 題目敘述 + benchmark + scorer + 環境（不含生成的 spec）。
4. **可重現**：記錄模型版本、編譯/執行指令、測資、verifier 結果、最佳化指標數值、時間/回合數。
5. **量化最佳化品質**：以數值記錄最佳化指標（cut size / 面積 / HPWL），對比 baseline。
6. **研究先行，超越基準**：寫 code 前 `research.md` 須記錄 baseline 門檻（各題 README 的 Min）、調研候選演算法、選定有潛力**超越**（非僅接近）baseline 的方案；未完成研究不得進入 tasks/implement。

## SDD 命令（Qwen Code）

`/speckit.specify` → `/speckit.plan` → `/speckit.tasks` → `/speckit.implement`
（選用 `/speckit.clarify`、`/speckit.analyze`）。跑 `/speckit.specify` 時以
`SPECIFY_FEATURE_DIRECTORY=problems/<NNN>/experiments/<model>/spec` 指定該模型的 SDD 目錄。
