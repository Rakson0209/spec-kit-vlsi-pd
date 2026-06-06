---
name: speckit-specify
description: Create feature spec for VLSI PD experiment. Use to start a new experiment.
metadata:
  triggers: speckit.specify, /speckit.specify, create spec, start new experiment
---

# speckit-specify

## 流程

1. 確定目錄：`problems/<NNN>/experiments/<model>/spec`
2. 讀 `reference/spec.pdf` + 問題 README.md（Min 門檻）
3. 讀 `.specify/templates/spec-template.md`
4. 產生 `spec.md`：問題描述、輸入輸出格式、限制條件、成功標準（對齊 Min）
5. 產生 `checklists/requirements.md`
6. 寫入 `.specify/feature.json`

## VLSI PD 注意
- Min 門檻在問題 README.md 的 "Baseline 指標門檻" 表
- 計分：`python scorer/score.py <num> --output-dir <dir>`
- API key 從環境變數讀，不可 hardcode
