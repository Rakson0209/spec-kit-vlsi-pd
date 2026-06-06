---
name: speckit-analyze
description: Cross-artifact consistency check. Use after speckit-tasks, before implement.
metadata:
  triggers: speckit.analyze, /speckit.analyze, analyze consistency, review artifacts
---

# speckit-analyze

檢查 `spec.md` ↔ `plan.md` ↔ `tasks.md` 一致性：
- 所有 spec 需求有對應 task
- 所有 task 有對應檔案路徑
- 無矛盾或遺漏
