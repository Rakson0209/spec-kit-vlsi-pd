---
description: "Task list for Fixed-outline Floorplanning (claude-opus-4-8)"
---

# Tasks: Fixed-outline Floorplanning Optimizer (claude-opus-4-8)

**Input**: Design documents from `problems/002-floorplanning/experiments/claude-opus-4-8/spec/`

**Prerequisites**: plan.md ✅、spec.md ✅、research.md ✅、data-model.md ✅、contracts/ ✅

**Tests**: 本專案無單元測試框架；正確性以官方 `benchmark/verifier/verify` 為真理、品質以 `scorer/score.py`
計分（憲章原則 II）。故不產生獨立單元測試任務，改以 verifier/scorer 作為各 story 的驗收。

**Organization**: 任務依 spec.md 三個優先級使用者故事分組。**注意**：本實作為**單一檔案** `main.cpp`，
絕大多數任務修改同一檔，**不可平行**（同檔衝突）；`[P]` 僅標於真正獨立檔案（Makefile、RESULT.md、docs）。

## Path Conventions

實作根目錄：`problems/002-floorplanning/experiments/claude-opus-4-8/`
- 主程式：`main.cpp`（單檔，內部以函式分區）
- 建置：`Makefile`
- 紀錄：`RESULT.md`
- 輸出：`out_sample.floorplan`、`public1~4.floorplan`
- 測資：`../../benchmark/testcase/`；驗證器：`../../benchmark/verifier/verify`；計分：專案根 `scorer/score.py`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: 建立建置骨架與檔案結構

- [x] T001 建立 `problems/002-floorplanning/experiments/claude-opus-4-8/Makefile`，內容為 `g++ -std=c++11 -O3 -o hw3 main.cpp`（target：`all`、`clean`）
- [x] T002 [P] 建立 `problems/002-floorplanning/experiments/claude-opus-4-8/RESULT.md` 骨架（依 `experiments/README.md` 模板：模型版本、編譯/執行指令、結果表、seed、回合數欄位待填）
- [x] T003 建立 `problems/002-floorplanning/experiments/claude-opus-4-8/main.cpp` 骨架：`#include`、`using namespace std`、`int main(int argc,char**argv)` 讀 `argv[1]`/`argv[2]` 並呼叫 `solve(input,output)` 空殼

**Checkpoint**: `make` 可編譯出空殼 `hw3`，能接受兩個命令列引數

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 所有 user story 共用的資料結構、解析、合法性判定、HPWL、輸出。**完成前任何 story 無法進行**

**⚠️ CRITICAL**: 此階段為阻塞前置，全部 story 依賴

- [x] T004 在 `main.cpp` 定義資料結構（依 data-model.md）：`struct Chip{int W,H;}`、`struct Module{string name; long area; int x,y,w,h; bool fixed;}`、`struct Net{int a,b,weight;}`（a/b 為 module index），及全域 `vector<Module>`、`vector<Net>`、名稱→index 的 `unordered_map`
- [x] T005 在 `main.cpp` 實作輸入解析 `parse(input)`：讀 `ChipSize W H`、`NumSoftModules`+`SoftModule name area`、`NumFixedModules`+`FixedModule name x y w h`、`NumNets`+`Net A B weight`；容忍空行；soft 與 fixed 統一存入 `vector<Module>`，net 端點解析成 index（依 contracts/io-format.md）
- [x] T006 在 `main.cpp` 實作 HPWL 計算 `long hpwl()`：對每 net 以整數中心 `cx=x+w/2, cy=y+h/2`，累加 `weight*(|cxA-cxB|+|cyA-cyB|)`（須與 verifier 一致，見 data-model.md）
- [x] T007 在 `main.cpp` 實作合法性判定子：`bool inOutline(m)`（`0≤x,0≤y,x+w≤W,y+h≤H`）、`bool overlap(a,b)`（矩形交集>0）、`bool arOK(m)`（`0.5≤w/h≤2.0`，整數保守判 `2*h≥w && 2*w≥h`）、`bool areaOK(m)`（`(long)w*h≥area`）、`bool anyOverlap()`（O(N²) 掃所有模組對）
- [x] T008 在 `main.cpp` 實作輸出 `write(output)`：首行 `Wirelength <hpwl()>`、空行、`NumSoftModules <n>`、各 soft module `name x y w h`（**不輸出 fixed**，依 contracts/io-format.md）
- [x] T009 在 `main.cpp` 串接最小管線 `solve()`：parse → （暫直接擺一個 trivial 合法初值）→ write，驗證讀寫格式正確

**Checkpoint**: 能解析全部測資、計算 HPWL、輸出合法格式檔（暫不要求高品質）

---

## Phase 3: User Story 1 - 產生合法平面規劃 (Priority: P1) 🎯 MVP

**Goal**: 對 sample + public1~4 產生通過 `verify` 的合法 `.floorplan`（不重疊、輪廓內、AR∈[0.5,2]、面積≥area、fixed 不動）

**Independent Test**: `../../benchmark/verifier/verify <input> <output>` 對 5 份測資全印 `[Success]`

### Implementation for User Story 1

- [x] T010 [US1] 在 `main.cpp` 實作形狀指派 `chooseShape(Module&)`：為 soft module 選整數 `w,h` 使 `w*h≥area` 且 `w/h∈[0.5,2]`（預設近正方 `w=ceil(sqrt(area))` 起，調整到滿足 AR 與面積下限）
- [x] T011 [US1] 在 `main.cpp` 實作建構式合法落子 `constructLegal()`：先標記 fixed 佔位，soft 依面積遞減以 shelf/skyline 由下而上、由左而右擺入輪廓內空位（繞開 fixed），每次落子即時通過 `inOutline`/`overlap`；此為**保證合法**的 fallback 路徑
- [x] T012 [US1] 在 `solve()` 串接 US1 管線：parse → `chooseShape` 全 soft → `constructLegal` → `write`；移除 T009 的 trivial 暫值
- [x] T013 [US1] 邊界處理：高密度/大面積模組落子失敗時的重試或縮 AR 策略，確保 5 份測資皆可擺下（spec.md Edge Cases）；0 net、純 fixed net 不影響合法性
- [x] T014 [US1] 編譯並對 sample + public1~4 執行，逐一以 `verify` 確認 `[Success]`；修正任何越界/重疊/AR/面積/Wirelength 不符問題

**Checkpoint**: 5 份測資全部 `[Success]`（SC-001 達成）— MVP 可交付

---

## Phase 4: User Story 2 - HPWL 低於 Min 門檻 (Priority: P2)

**Goal**: 在合法前提下將加權 HPWL 壓到 ≤ 各 testcase Min 門檻（保底 < Reference 且 < Max）

**Independent Test**: 通過 `verify` 後以 `scorer/score.py` 取 HPWL，對照 research.md 門檻表（public1≤161.6M…）

### Implementation for User Story 2

- [x] T015 [US2] 在 `main.cpp` 實作線長導向全域擺放 `globalPlace()`：將每 soft module 視為點（中心），以力導向鬆弛迭代——每步把模組中心朝其加權相連模組（含 fixed 錨點）的加權質心移動；輸出各 soft 的目標中心（浮點，暫允許重疊）
- [x] T016 [US2] 在 `main.cpp` 實作合法化 `legalize()`：依目標中心把 soft 轉成整數矩形，按目標位置排序貪婪落子 / 推擠去重疊並夾回輪廓內，全程維持 `inOutline`/`overlap`/`arOK`/`areaOK`；失敗則回退 T011 `constructLegal`
- [x] T017 [US2] 在 `main.cpp` 實作 HPWL-only 模擬退火 `anneal()`：cost=`hpwl()`（**無面積項**）；算子=平移 / 兩模組交換位置 / 換形（`chooseShape` 變體）/ 朝連線質心拉近；每步以 O(N²) `anyOverlap` + 約束判定確保合法，Metropolis 接受（`exp(-Δ/T)`），降溫排程；全程保留歷史最佳合法解
- [x] T018 [US2] 在 `solve()` 串接 US2 管線：parse → `chooseShape` → `globalPlace` → `legalize` →（失敗則 `constructLegal`）→ `anneal` → `write`
- [x] T019 [US2] 對 public1~4 執行 + `verify` + `score.py`，記錄 HPWL；對未達 Min 者調參（降溫率、初溫、迭代數、算子機率、力導向步數）逼近並超越 Min
- [x] T020 [US2] 驗證仍 100% 合法（`verify` 全 `[Success]`），且 HPWL 至少 < Reference 且 < Max（SC-002 / SC-003）

**Checkpoint**: public1~4 HPWL ≤ Min（或保底優於 Reference），且全部仍合法

---

## Phase 5: User Story 3 - 在時間上限內完成 (Priority: P3)

**Goal**: 每份測資 ≤ 600s（public2/3/4 目標 ≤ 120s），且結果可重現

**Independent Test**: wall-clock 計時各 public testcase，皆 ≤ 600s 並輸出合法結果

### Implementation for User Story 3

- [x] T021 [US3] 在 `main.cpp` 加入時間預算控制：以 `clock()` 計時，`anneal()` 與 `globalPlace()` 在約 580s 前終止並輸出歷史最佳（含安全餘裕給 `write`）
- [x] T022 [US3] 在 `main.cpp` 固定隨機 seed（`srand(固定值)`）並於程式註解記錄，使同硬體重跑 HPWL 差異 ≤ 5%（SC-005）
- [x] T023 [US3] 調整退火/迭代規模使 public2/3/4 在 ≤ 120s 收斂、public1 在 ≤ 600s 收斂（依測資規模自適應迭代預算）
- [x] T024 [US3] 計時重跑 public1~4，記錄各自 wall-clock，確認皆 ≤ 600s 且 verifier 仍 `[Success]`

**Checkpoint**: 全部測資合法、達 HPWL 目標、且在時間上限內

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: 跨 story 收尾與紀錄

- [x] T025 對 sample + public1~4 完整重跑：`verify`（全 `[Success]`）+ `scorer/score.py`（HPWL）+ 計時，作為最終數據
- [x] T026 填寫 `problems/002-floorplanning/experiments/claude-opus-4-8/RESULT.md`：模型版本、編譯/執行指令、各測資 verifier 結果 / HPWL / 時間、對照 Min/Reference/Max、seed、開發回合數（憲章原則 IV）
- [x] T027 [P] 依 `spec/quickstart.md` 走一遍端到端驗證流程，確認指令與預期輸出一致
- [x] T028 `main.cpp` 程式碼整理：函式分區註解、移除暫用程式碼、確認 `g++ -std=c++11 -O3` 無警告

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: 無依賴，可立即開始
- **Foundational (Phase 2)**: 依賴 Setup；**阻塞所有 user story**
- **US1 (Phase 3)**: 依賴 Foundational；交付 MVP（合法輸出）
- **US2 (Phase 4)**: 依賴 Foundational；**且依賴 US1 的 `chooseShape`/`constructLegal`**（作為 legalize 的 fallback 與形狀指派）
- **US3 (Phase 5)**: 依賴 US2 的 `anneal`/`globalPlace`（為其加上時間/seed 控制）
- **Polish (Phase 6)**: 依賴 US1–US3 完成

### User Story Dependencies（本專案為單檔演算法，story 間非完全獨立）

- **US1 (P1)**: Foundational 後即可，獨立可驗（verifier `[Success]`）→ MVP
- **US2 (P2)**: 在 US1 合法基礎上加全域擺放 + SA 提升品質；重用 US1 的形狀/合法化
- **US3 (P3)**: 在 US2 演算法上加時間預算與 seed；不改變正確性

### Within Each Story

- Foundational 先於所有 story；US1 先於 US2 先於 US3（線性依賴，因共用單檔與漸進管線）
- 每 checkpoint 以 `verify` 驗合法、`score.py` 量 HPWL

### Parallel Opportunities

- T002（RESULT.md）、T027（quickstart 驗證文件）標 `[P]`：獨立檔案，可與 `main.cpp` 任務並行
- **其餘任務皆改同一個 `main.cpp`，不可平行**（同檔衝突）——這是單檔實作的刻意取捨

---

## Parallel Example

```bash
# 僅獨立檔案任務可並行：
Task: "T002 建立 RESULT.md 骨架"
Task: "T001 建立 Makefile"
# main.cpp 內部任務（T003–T024）須循序，避免同檔衝突
```

---

## Implementation Strategy

### MVP First (User Story 1)

1. Phase 1 Setup → 2. Phase 2 Foundational（阻塞）→ 3. Phase 3 US1
4. **STOP & VALIDATE**：5 份測資 `verify` 全 `[Success]`（合法 MVP）

### Incremental Delivery

1. Setup + Foundational → 地基就緒
2. US1 → 合法輸出（MVP，SC-001）
3. US2 → HPWL ≤ Min（SC-002/003，核心競爭力）
4. US3 → 時間 / 重現性（SC-004/005）
5. Polish → 數據彙整入 RESULT.md

### 對齊憲章

- 每 story checkpoint 以 verifier 為真理（原則 II）；最終以 HPWL 數值對照 Min/Reference（原則 V）
- RESULT.md 記錄完整可重現資訊（原則 IV）；超越 Min 為第一優先（原則 VI）

---

## Notes

- `[P]` = 不同檔、無依賴；本專案僅 RESULT.md / Makefile / quickstart 驗證屬之
- `[Story]` 標籤對應 spec.md 使用者故事，便於追溯
- 正確性永遠以 `verify` 為準；未過即未通過，無論程式看似多合理
- 建議每完成一個任務或邏輯群組即 commit
- 可在任一 checkpoint 停下獨立驗證
