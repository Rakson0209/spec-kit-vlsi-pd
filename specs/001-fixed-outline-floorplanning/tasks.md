# Tasks: 固定輪廓平面規劃 (Fixed-outline Floorplanning)

**Input**: Design documents from `/specs/001-fixed-outline-floorplanning/`

**Prerequisites**: plan.md ✅, spec.md ✅, research.md ✅, data-model.md ✅, contracts/io-format.md ✅

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization and basic structure

- [x] T001 Create project directory `problems/002-floorplanning/experiments/qwen/src/`
- [x] T002 Create `problems/002-floorplanning/experiments/qwen/Makefile` with `g++ -std=c++11 -O3` compile rule, output to `../bin/hw3`
- [x] T003 Create `problems/002-floorplanning/experiments/qwen/out/` output directory and `.gitkeep`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core data structures and input parsing that ALL user stories depend on

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [x] T004 [P] Create `src/module.h` — Module struct
- [x] T005 [P] Create `src/net.h` — Net struct
- [x] T006 [P] Create `src/grid.h` — Grid class
- [x] T007 Create `src/solution.h` — Solution struct
- [x] T008 Create `src/parser.h` and `src/parser.cpp` — parse `.txt` input
- [x] T009 Create `src/output.cpp` and `src/output.h` — write `.floorplan` output

**Checkpoint**: Foundation ready — can parse input and write output; user story implementation can now begin

---

## Phase 3: User Story 1 — 讀取平面規劃輸入並產生合法解 (Priority: P1) 🎯 MVP

**Goal**: 讀取 `.txt` 輸入、在固定輪廓內為每個軟模組分配位置與尺寸、產出不重疊的合法 `.floorplan` 輸出

**Independent Test**: 對 `sample.txt` 執行，輸出全部軟模組且不重疊、在輪廓內

### Implementation for User Story 1

- [x] T010 [US1] Create `src/placement.cpp` and `src/placement.h`
- [x] T011 [US1] Implement `Grid::can_place()`
- [x] T012 [US1] Implement `Grid::set()`
- [x] T013 [US1] Implement `Module::generate_shapes()`
- [x] T014 [US1] Implement placement retry loop
- [x] T015 [US1] Create `src/main.cpp`
- [x] T016 [US1] Implement `Solution::compute_wirelength()`
- [x] T017 [US1] Implement `Solution::is_valid()`

**Checkpoint**: At this point, running `./hw3 sample.txt out.floorplan` should produce a valid, non-overlapping floorplan

---

## Phase 4: User Story 2 — 滿足長寬比限制 (Priority: P1)

**Goal**: 每個軟模組的 h/w 必須落在 [0.5, 2] 範圍內，放置與 reshape 時自動確保

**Independent Test**: 對輸出中每個軟模組計算 h/w，確認全部在 [0.5, 2]

### Implementation for User Story 2

- [x] T018 [P] [US2] Implement `Module::is_valid_shape()`
- [x] T019 [US2] Add aspect ratio validation in placement
- [x] T020 [US2] Add aspect ratio validation in reshape operation
- [x] T021 [US2] Add edge case handling

**Checkpoint**: All soft modules in output satisfy both area and aspect ratio constraints

---

## Phase 5: User Story 3 — 最小化加權線長 (Priority: P2)

**Goal**: 在合法前提下，透過模擬退火演算法盡量減少加權 HPWL

**Independent Test**: 對 `sample.txt` 執行，HPWL ≤ 215；與參考實作比較

### Implementation for User Story 3

- [x] T022 [P] [US3] Create `src/optimizer.h`
- [x] T023 [P] [US3] Create `src/hpwl.h` and `src/hpwl.cpp`
- [x] T024 [US3] Implement `compaction`
- [x] T025 [US3] Implement neighbor operation — Swap
- [x] T026 [US3] Implement neighbor operation — Move
- [x] T027 [US3] Implement neighbor operation — Reshape
- [x] T028 [US3] Implement simulated annealing loop
- [x] T029 [US3] Implement cost function
- [x] T030 [US3] Implement backup/restore with dirty-cell tracking
- [x] T031 [US3] Implement RNG — `std::mt19937` with configurable seed
- [x] T032 [US3] Integrate optimizer into `main.cpp` flow

**Checkpoint**: HPWL for `sample.txt` should be competitive with reference solution; output remains valid

---

## Phase 6: User Story 4 — 批次處理多個測試案例 (Priority: P3)

**Goal**: 對所有 testcase 逐一執行並產生對應的 `.floorplan` 輸出

**Independent Test**: 對 5 個 testcase 執行，全部產出合法的 `.floorplan`

### Implementation for User Story 4

- [x] T033 [US4] Add timeout handling in optimizer
- [x] T034 [US4] Add graceful degradation
- [x] T035 [US4] Add memory handling for large cases — dirty-cell tracking
- [x] T036 [US4] Create `RESULT.md`

**Checkpoint**: All 5 testcases produce valid `.floorplan` files; scorer produces full results table

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Improvements that affect all user stories

- [x] T037 [P] Add compilation guard (`#pragma once`) to all `.h` files
- [x] T038 Add error handling
- [x] T039 Add progress logging
- [x] T040 Verify cross-platform compilation
- [x] T041 Run full validation — all 5 testcases valid (sample=215, public1=245274640, public2=34550821, public3=3082522, public4=117192850)
- [x] T042 Update `RESULT.md` with final scoring data

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion — BLOCKS all user stories
- **User Story 1 (Phase 3)**: Depends on Foundational — produces MVP
- **User Story 2 (Phase 4)**: Depends on Foundational, integrates with US1 placement
- **User Story 3 (Phase 5)**: Depends on US1 + US2 (needs valid placement to optimize)
- **User Story 4 (Phase 6)**: Depends on US3 (needs full optimizer to run on all cases)
- **Polish (Phase 7)**: Depends on all user stories

### Parallel Opportunities

- T004, T005, T006 can be created in parallel (independent header files)
- T022, T023 can be created in parallel (optimizer header + HPWL utility)
- T025, T026, T027 can be implemented in parallel (three independent neighbor operations)

### Within Each User Story

- Data structures before algorithms
- Core placement before optimization
- Single testcase validation before batch processing

---

## Implementation Strategy

### MVP First (Phase 1 → 2 → 3)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (CRITICAL — blocks all stories)
3. Complete Phase 3: User Story 1 — greedy placement producing valid floorplan
4. **STOP and VALIDATE**: Run against `sample.txt`, verify output is valid
5. If valid, MVP is ready

### Incremental Delivery

6. Add Phase 4: Aspect ratio enforcement — ensure all shapes comply
7. Add Phase 5: Simulated annealing optimization — minimize HPWL
8. Add Phase 6: Batch processing — run all testcases
9. Add Phase 7: Polish — error handling, logging, cross-platform
