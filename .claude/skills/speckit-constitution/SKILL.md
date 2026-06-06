---
name: speckit-constitution
description: Manage project constitution.
metadata:
  triggers: speckit.constitution, /speckit.constitution, update constitution
---

# speckit-constitution

讀/寫 `.specify/memory/constitution.md`。修訂後同步檢查：
- `.specify/templates/plan-template.md` — Constitution Check
- `CLAUDE.md` — principles summary
- 版本號依 semver 更新
