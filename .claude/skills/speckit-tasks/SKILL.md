---
name: speckit-tasks
description: Generate tasks.md from plan. Use after speckit-plan.
metadata:
  triggers: speckit.tasks, /speckit.tasks, create tasks, task breakdown
---

# speckit-tasks

## 流程

1. 讀 `plan.md` + `spec.md`
2. 讀 `.specify/templates/tasks-template.md`
3. 產生 `tasks.md`：`- [ ] T### [P?] Description with file path`

## Phase 結構
- Setup: Makefile, main.cpp, 目錄結構
- Foundational: parser, data structures (Module, Net, Cell)
- Core: 演算法實作
- Output: writer
- Verify: scorer 驗證全 case

## 規則
- 每個 task 一行，含具體檔案路徑
- `[P]` = 可平行執行
- 依賴關係正確
