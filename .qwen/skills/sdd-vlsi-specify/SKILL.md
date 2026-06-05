---
name: sdd-vlsi-specify
description: Write SDD specs for VLSI physical design problems by extracting validation rules from scorer code and constitution principles
source: auto-skill
extracted_at: '2026-06-05T07:38:18.462Z'
---

# SDD Specify for VLSI Physical Design Problems

## When to Use

When running `/speckit.specify` for any problem under `problems/NNN-*` in this VLSI physical design automation project.

## Approach

### 1. Gather Ground-Truth Sources (Before Writing)

Read these files in order — they define the actual constraints, not just the problem description:

| Source | Path | What to Extract |
|---|---|---|
| **Scorer code** | `scorer/lib/<problem>.py` | Exact validation logic: overlap detection, aspect ratio bounds, area constraints, center-point calculation (e.g. `x + w // 2`), HPWL formula |
| **Constitution** | `.specify/memory/constitution.md` | Principles that shape requirements — especially Principle VII (legality gate before optimization) and Principle VI (beat baseline) |
| **Problem README** | `problems/NNN-*/README.md` | Baseline threshold table with Min values for each testcase |
| **Spec template** | `.specify/templates/spec-template.md` | Required section structure |
| **Test cases** | `problems/NNN-*/benchmark/testcase/*.txt` | Input format patterns, data scale |

### 2. Translate Scorer Rules into Functional Requirements

For each validation check in the scorer code, create a corresponding FR:

- **Overlap detection** (`_overlap()` function) → "所有模組互不重疊（標準矩形相交測試）"
- **Boundary check** (`x < 0 or y < 0 or x+w > chip_w...`) → "所有模組落在輪廓內"
- **Area constraint** (`w * h < min_area`) → "軟模組 w*h >= 指定最小面積"
- **Aspect ratio** (`h/w ∈ [0.5, 2.0]`) → "軟模組 h/w 在 0.5~2.0 之間"
- **Center calculation** (`x + w // 2`) → "中心座標向下取整（floor），非四捨五入"
- **HPWL formula** (`weight * (|cx1-cx2| + |cy1-cy2|)`) → 精確記錄計算方式

### 3. Structure Success Criteria from Baseline Table

Each testcase's Min threshold from the README becomes a separate, measurable SC:

```
- SC-001: 全部 testcase 經 scorer 驗證 100% 合法
- SC-002: <testcase> wirelength/hpwl <= <Min value from README>
```

Emphasize the **legality gate** (Principle VII) as the first and highest-priority success criterion — all testcases must be 100% legal before optimization matters.

### 4. User Stories Aligned with Constitution

Structure stories by priority matching the constitution's hierarchy:

- **P1**: 合法性（Principle VII — gate）— 所有合法性條件
- **P2**: 最佳化品質（Principle V, VI — objective）— 壓低指標到 Min 以下
- **P3**: 可重現性與覆蓋（Principle IV）— 全部 testcase、可重複執行

### 5. Document Assumptions

Capture non-obvious assumptions derived from scorer behavior:

- 座標/尺寸為整數（scorer 以 `int()` 解析）
- 輸出格式含/不含固定模組
- Net 僅涉及兩模組（從輸入格式推斷）
- 執行環境（Python 3 可跑 scorer、g++ 可編譯）

### 6. Validate Against Quality Checklist

Create `checklists/requirements.md` and verify all 20 items. Key failure points to watch for:

- **Implementation details leaking**: Don't specify the algorithm (simulated annealing, ILP, etc.) — that belongs in `research.md` during `/speckit.plan`
- **Missing testability**: Every FR must be checkable by the scorer
- **Vague success criteria**: Use exact numbers from README, not "approximately" or "close to"

## Pitfalls to Avoid

- ❌ Writing algorithm choices in spec.md (belongs in research.md during plan phase)
- ❌ Approximating scorer rules from PDF alone (read the Python source)
- ❌ Treating legality and optimization as equal priorities (legality is a gate)
- ❌ Omitting edge cases the scorer will reject (aspect ratio boundary, area exactly-at-minimum)
- ❌ Confusing output format: `.floorplan` contains only soft modules, not fixed ones

## Output Artifacts

- `spec.md` — the specification
- `checklists/requirements.md` — quality checklist (all items checked)
- `.specify/feature.json` — updated with feature directory path

## Related Skills

- `sdd-vlsi-plan-tasks` — Research-driven plan + task generation for VLSI problems (next step after specify)
- `sdd-vlsi-implement` — Implement VLSI solutions with scorer-verified legality gate + scorer optimization loop
