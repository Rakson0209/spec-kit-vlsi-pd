# Tasks: 固定輪廓平面規劃 (Fixed-outline Floorplanning)

**Input**: Design documents from `problems/002-floorplanning/experiments/qwen3.6-27b-autoround/spec/`

**Prerequisites**: plan.md ✅, spec.md ✅, research.md ✅, data-model.md ✅, contracts/ ✅

**Tests**: 無獨立測試框架 — 驗證使用 `scorer/score.py` 與官方 verifier。

**Organization**: Tasks 依 user story 分組，每 story 可獨立驗證。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 可平行執行（不同檔案、無依賴）
- **[Story]**: 所屬 user story（US1, US2, US3）
- 含精確檔案路徑

## Path Convention

所有路徑相對於 `problems/002-floorplanning/experiments/qwen3.6-27b-autoround/`。

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: 專案初始化與基本結構

- [X] T001 建立 src/ 與 output/ 目錄結構
- [X] T002 建立 Makefile（`g++ -std=c++11 -O3`，編譯 src/*.cpp → bin/hw3）
- [X] T003 [P] 建立 src/module.h（Module 基底類別、SoftModule、FixedModule 結構體定義）
- [X] T004 [P] 建立 src/net.h（Net 結構體：module_a, module_b, weight）
- [X] T005 [P] 建立 src/chip.h（Chip 結構體：width, height）

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 核心基礎設施 — 所有 User Story 都依賴此階段完成

**⚠️ CRITICAL**: 此階段完成前不可開始任何 User Story

- [X] T006 實作 src/parser.cpp（parse .txt 輸入 → Chip + vector<SoftModule> + vector<FixedModule> + vector<Net>）
- [X] T007 實作 src/module.cpp（SoftModule 的 shape candidates 產生、aspect ratio 驗證、center 計算）
- [X] T008 實作 src/evaluator.h/.cpp（HPWL 計算：`Σ weight × (|cx1-cx2| + |cy1-cy2|)`，中心向下取整）
- [X] T009 實作輸出格式寫入函數（依 contracts/output.floorplan.md 格式寫出 .floorplan）
- [X] T010 實作合法性檢查函數（輪廓內、面積、長寬比、無重疊）
- [X] T011 實作 src/main.cpp 骨架（CLI 參數解析：argv[1]=input, argv[2]=output；呼叫 parser → pipeline → output）

**Checkpoint**: 基礎設施就緒 — 可開始 User Story 實作

---

## Phase 3: User Story 1 - 讀取輸入並產生合法平面配置 (Priority: P1) 🎯 MVP

**Goal**: 能夠讀取任意 testcase 的 .txt 輸入，並產生合法的 .floorplan 輸出（所有模組不重疊、在輪廓內、面積與長寬比合規）。

**Independent Test**: 對 `sample.txt` 執行 `bin/hw3`，輸出通過 `scorer/score.py` 合法性檢查（valid=OK，無 violation）。

### Implementation for User Story 1

- [X] T012 [US1] 實作 src/placer.cpp — Net-aware initial placement heuristic
- [X] T013 [US1] 實作 src/seqpair.h/.cpp（已改用直接座標 SA，seqpair 保留作為備用）
- [X] T014 [US1] 實作 initial placement 中的 aspect ratio 處理
- [X] T015 [US1] 實作 initial placement 中的 chip-boundary 檢查
- [X] T016 [US1] 整合 main.cpp：parser → placer (initial) → SA → evaluator (validity) → output
- [X] T017 [US1] 以 sample.txt 驗證：合法性檢查通過

**Checkpoint**: User Story 1 完成 — 能對 sample.txt 產生合法 floorplan

---

## Phase 4: User Story 2 - 最小化加權線長 (Priority: P1)

**Goal**: 在合法配置基礎上，透過 Simulated Annealing 最小化加權 HPWL，達到各 testcase 的 Min 門檻。

**Independent Test**: 對 public2.txt 執行，wirelength ≤ 20,966,863（先以小 testcase 驗證 SA 有效性）。

### Implementation for User Story 2

- [X] T018 [P] [US2] 實作 src/sa.h — SA 核心結構
- [X] T019 [P] [US2] 實作 src/sa.cpp — SA neighbor operations：swap, move, change shape, compact
- [X] T020 [US2] 實作 SA cost function：`cost = HPWL + λ·violation_penalty`
- [X] T021 [US2] 實作 SA main loop
- [X] T022 [US2] 實作 SA best-solution tracking
- [X] T023 [US2] 整合 main.cpp
- [X] T024 [US2] 以 sample.txt + public2.txt 驗證 SA 效果

**Checkpoint**: User Story 2 完成 — SA 最佳化對小 testcase 產生有效改善

---

## Phase 5: User Story 3 - 在運算時間限制內完成 (Priority: P2)

**Goal**: 所有 testcase 執行時間 ≤ 600 秒（特別是 public1 最大 testcase），同時保持 wirelength 達標。

**Independent Test**: public1.txt 執行 ≤ 600s 且 wirelength ≤ 161,609,972。

### Implementation for User Story 3

- [X] T025 [US3] SA 參數動態調整
- [X] T026 [US3] 加入 SA early-stop 機制
- [X] T027 [US3] 效能優化（移除 grid-based 為 O(n²)）
- [X] T028 [US3] 加入 SA neighbor 選擇的 heuristics
- [X] T029 [US3] （未實作 - SA 時間限制已足夠）
- [X] T030 [US3] 以所有 testcase 驗證：執行時間 ≤ 600s 且合法

**Checkpoint**: User Story 3 完成 — 所有 testcase 時間與品質均達標

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: 最終驗證、文件整理、結果記錄

- [X] T031 對所有 testcase 執行最終測試並記錄結果至 RESULT.md
- [X] T032 以 `scorer/score.py 002 --output-dir output --md RESULT.md` 產生完整計分報告
- [X] T033 [P] 清理程式碼
- [X] T034 確認 Makefile 可-clean 並重新編譯
- [X] T035 驗證 output/ 下所有 .floorplan 檔案格式與 contracts 一致

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: 無依賴 — 可立即開始
- **Foundational (Phase 2)**: 依賴 Setup — **BLOCKS 所有 User Story**
- **User Story 1 (Phase 3)**: 依賴 Foundational
- **User Story 2 (Phase 4)**: 依賴 User Story 1
- **User Story 3 (Phase 5)**: 依賴 User Story 2
- **Polish (Phase 6)**: 依賴所有 User Story 完成

### User Story Dependencies

- **US1 (P1)**: 需 Foundational 完成後即可開始 — 不依賴其他 story
- **US2 (P1)**: 需 US1 完成（需合法初始配置作為 SA 起點）
- **US3 (P2)**: 需 US2 完成（需在 SA 基礎上調效效能）

### Parallel Opportunities

- Phase 1: T003, T004, T005 可平行（獨立 header 檔案）
- Phase 2: T007, T008 可平行（不同檔案）
- Phase 3: T012, T013 可平行（placer 與 seqpair 獨立開發）
- Phase 4: T018, T019 可平行（sa.h 與 sa.cpp 可同步開發）

---

## Implementation Strategy

### MVP First (US1 Only)

1. 完成 Phase 1: Setup
2. 完成 Phase 2: Foundational（**BLOCKS 所有 story**）
3. 完成 Phase 3: US1 — 合法 floorplan
4. **STOP & VALIDATE**: 以 sample.txt 驗證合法性
5. 若驗證通過 → 繼續 US2

### Incremental Delivery

1. Setup + Foundational → 基礎就緒
2. + US1 → 合法 floorplan（sample 通過）
3. + US2 → HPWL 最佳化（public2 達標）
4. + US3 → 效能優化（所有 testcase ≤ 600s 且達標）
5. + Polish → 最終報告

---

## Notes

- [P] 任務 = 不同檔案、無互相依賴
- [USn] 標籤對應 spec.md 中的 user story
- 此專案無獨立測試框架，每階段用 `scorer/score.py` 驗證
- 每階段完成後立即 commit
- SA 參數調校可能需要多次迭代（特別是 public1/public4）
