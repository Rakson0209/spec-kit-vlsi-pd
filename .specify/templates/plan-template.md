# Implementation Plan: [FEATURE]

**Branch**: `[###-feature-name]` | **Date**: [DATE] | **Spec**: [link]

**Input**: Feature specification from `/specs/[###-feature-name]/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

[Extract from feature spec: primary requirement + technical approach from research]

## Technical Context

<!--
  ACTION REQUIRED: Replace the content in this section with the technical details
  for the project. The structure here is presented in advisory capacity to guide
  the iteration process.
-->

**Language/Version**: [e.g., Python 3.11, Swift 5.9, Rust 1.75 or NEEDS CLARIFICATION]

**Primary Dependencies**: [e.g., FastAPI, UIKit, LLVM or NEEDS CLARIFICATION]

**Storage**: [if applicable, e.g., PostgreSQL, CoreData, files or N/A]

**Testing**: [e.g., pytest, XCTest, cargo test or NEEDS CLARIFICATION]

**Target Platform**: [e.g., Linux server, iOS 15+, WASM or NEEDS CLARIFICATION]

**Project Type**: [e.g., library/cli/web-service/mobile-app/compiler/desktop-app or NEEDS CLARIFICATION]

**Performance Goals**: [domain-specific, e.g., 1000 req/s, 10k lines/sec, 60 fps or NEEDS CLARIFICATION]

**Constraints**: [domain-specific, e.g., <200ms p95, <100MB memory, offline-capable or NEEDS CLARIFICATION]

**Scale/Scope**: [domain-specific, e.g., 10k users, 1M LOC, 50 screens or NEEDS CLARIFICATION]

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

基於 `.specify/memory/constitution.md` 憲章，以下守則必須在計畫階段通過：

| # | 守則 | 驗證項目 | 狀態 |
|---|------|---------|------|
| I | **規格先行** | spec.md 是否存在且涵蓋輸入/輸出格式、限制條件、最佳化目標、驗收標準 | ⬜ |
| II | **驗證器即真理** | 是否以 `benchmark/verifier/verify` 為正確性依據，不依赖人工或模型自評 | ⬜ |
| III | **公平比較** | 實作是否獨立放在 `experiments/<model>/`，不與其他模型共用中間產物 | ⬜ |
| IV | **可重現** | 是否記錄：模型版本、SDD 產物、編譯/執行指令、測資、verifier 結果、最佳化數值、時間/回合數 | ⬜ |
| V | **量化最佳化品質** | 是否定義明確最佳化目標並記錄數值，作為跨模型比較依據 | ⬜ |
| VI | **研究先行，超越基準** | `research.md` 是否已：(1) 記錄 baseline 指標門檻；(2) 調研候選演算法與取捨；(3) 選定有潛力超越 baseline 的方案。未完成不得進入 tasks/implement | ⬜ |

**編譯基準**: 專案內建 portable `g++ -std=c++20 -O3`（`tools\mingw64\`，見 `setup-env.bat`）；003 須連結 `reference/obj/*.o`。
**加速選項**（內建編譯器已支援）：`-fopenmp`（OpenMP 平行）、`-pthread`（多執行緒）、`-I tools/boost`（Boost，需自行下載）。
**評測三項數據**: 通過/失敗、最佳化指標、時間/開發回合數，缺一不可。

**最佳化優先級**（計畫設計時必須遵守）:
1. **結果品質優先**（最重要）— 設計決策必須優先追求**達到並超越基準線**的最佳化指標數值（cut size 更小、HPWL 更低、面積更緊湊…）；以「超越 baseline」為目標，而非僅接近。演算法選擇、資料結構、啟發式策略皆以此為第一考量。先在 `research.md` 記下 baseline 的指標數值作為要打敗的門檻。
2. **執行效率次之** — 在結果品質相同或相近的前提下，優先選擇時間複雜度更低、記憶體使用更少、實際執行更快的方案。避免不必要的 O(n²) 嵌套迴圈或冗餘資料拷貝。

> 若有守則無法通過，必須在下方 Complexity Tracking 表格說明理由並提出變通方案。

## Project Structure

### Documentation (this feature)

```text
specs/[###-feature]/
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (/speckit.plan command)
├── data-model.md        # Phase 1 output (/speckit.plan command)
├── quickstart.md        # Phase 1 output (/speckit.plan command)
├── contracts/           # Phase 1 output (/speckit.plan command)
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)
<!--
  ACTION REQUIRED: Replace the placeholder tree below with the concrete layout
  for this feature. Delete unused options and expand the chosen structure with
  real paths (e.g., apps/admin, packages/something). The delivered plan must
  not include Option labels.
-->

```text
# [REMOVE IF UNUSED] Option 1: Single project (DEFAULT)
src/
├── models/
├── services/
├── cli/
└── lib/

tests/
├── contract/
├── integration/
└── unit/

# [REMOVE IF UNUSED] Option 2: Web application (when "frontend" + "backend" detected)
backend/
├── src/
│   ├── models/
│   ├── services/
│   └── api/
└── tests/

frontend/
├── src/
│   ├── components/
│   ├── pages/
│   └── services/
└── tests/

# [REMOVE IF UNUSED] Option 3: Mobile + API (when "iOS/Android" detected)
api/
└── [same as backend above]

ios/ or android/
└── [platform-specific structure: feature modules, UI flows, platform tests]
```

**Structure Decision**: [Document the selected structure and reference the real
directories captured above]

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| [e.g., 4th project] | [current need] | [why 3 projects insufficient] |
| [e.g., Repository pattern] | [specific problem] | [why direct DB access insufficient] |
