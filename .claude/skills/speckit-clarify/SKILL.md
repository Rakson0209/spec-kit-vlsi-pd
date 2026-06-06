---
name: speckit-clarify
description: Ask targeted clarification questions. Use after speckit-specify, before speckit-plan.
metadata:
  triggers: speckit.clarify, /speckit.clarify, clarify spec, resolve ambiguities
---

# speckit-clarify

讀 `spec.md`，找出模糊處（未指定演算法、格式歧義、邊界條件），逐條提問。使用者回答後回填 spec。
