---
name: speckit-plan
description: Generate implementation plan with research. Use after speckit-specify.
metadata:
  short-description: Research + plan
  triggers: speckit.plan, /speckit.plan, create plan, implementation plan
---

# speckit-plan

## Phase 0: Research（必須先完成）

`research.md` 必須記錄：
1. baseline 指標門檻（取自各題 README 的 Min）
2. ≥2 個候選演算法，**優先選用 Boost 可加速的方案**（`boost::graph`、`boost::geometry`）
3. 選定方案 + 理由，必須有潛力超越 baseline

## Phase 1: Design

1. `plan.md` — Tech Context: C++20 + Boost + OpenMP + pthread
2. `data-model.md` — 資料結構
3. `contracts/` — I/O 格式
4. `quickstart.md` — 編譯執行指令

## 編譯基準

一律使用：`g++ -std=c++20 -O3 -fopenmp -pthread -I tools/boost`

## 驗證

- [ ] research.md 完整（baseline 門檻 + ≥2 演算法 + 選定理由）
- [ ] plan.md Technical Context 無遺漏
- [ ] 所有設計假設 Boost + 平行計算
