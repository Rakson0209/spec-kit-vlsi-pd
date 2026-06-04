# Feature Specification: 固定輪廓平面規劃 (Fixed-outline Floorplanning)

**Feature Branch**: `001-fixed-outline-floorplanning`

**Created**: 2026-06-04

**Status**: Draft

**Input**: User description: "實作 002 固定輪廓平面規劃 (fixed-outline floorplanning)。題目與評分定義見 problems/002-floorplanning/reference/spec.pdf 與該資料夾 README。輸入格式 .txt（ChipSize / SoftModule / FixedModule / Net），輸出 .floorplan。目標：在固定輪廓內擺放模組不重疊，最小化加權 HPWL。測資在 problems/002-floorplanning/benchmark/testcase/，驗收用 scorer/score.py 計算 wirelength 並檢查合法性。"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - 讀取平面規劃輸入並產生合法解 (Priority: P1)

使用者提供包含晶片輪廓、軟模組、硬模組與網路連接的 `.txt` 輸入檔案，系統讀取後在固定輪廓內為每個軟模組分配位置與尺寸，產生所有模組不重疊且在輪廓內的 `.floorplan` 輸出檔案。

**Why this priority**: 這是核心功能——沒有合法解就無法進入最佳化階段。此故事產生完整、可驗證的輸出。

**Independent Test**: 可完全透過對 `sample.txt` 執行、再以 verifier 確認 `[Success]` 來獨立測試，並產出有效的 `.floorplan` 檔案。

**Acceptance Scenarios**:

1. **Given** 輸入檔案包含晶片輪廓與軟/硬模組定義，**When** 系統處理完成，**Then** 輸出的 `.floorplan` 檔案包含每個軟模組的 `(name, x, y, w, h)` 資訊
2. **Given** 輸出檔案中的軟模組位置，**When** 以 verifier 驗證，**Then** 所有模組不重疊、全部在晶片輪廓內、且硬模組位置與輸入一致
3. **Given** 輸入檔案中每個軟模組有指定最小面積，**When** 系統分配尺寸，**Then** 每個軟模組的面積 `w × h` 大於或等於其最小面積

---

### User Story 2 - 滿足長寬比限制 (Priority: P1)

每個軟模組的長寬比 `h/w` 必須落在 `[0.5, 2]` 範圍內，系統在分配尺寸時自動確保此條件。

**Why this priority**: 這是課程題目的硬性限制——不滿足長寬比的解會被 verifier 判為不合法。

**Independent Test**: 對輸出檔案中每個軟模組計算 `h/w`，確認全部落在 `[0.5, 2]` 範圍。

**Acceptance Scenarios**:

1. **Given** 某軟模組最小面積為 25，**When** 系統分配尺寸，**Then** 該模組的 `h/w` 在 `[0.5, 2]` 範圍內且 `w × h ≥ 25`
2. **Given** 某軟模組最小面積為 1，**When** 系統分配尺寸，**Then** 仍滿足長寬比限制（即使為 1×1 也是合法）

---

### User Story 3 - 最小化加權線長 (Priority: P2)

使用者希望系統產出的解在合法前提下盡量減少加權 HPWL（Half-Perimeter Wire Length），以獲得更佳的線長指標。

**Why this priority**: 合法性是底線，線長最佳化是進階目標——先有合法解，再追求更好的數值。

**Independent Test**: 對輸出檔案以計分器重新計算加權 HPWL，並與參考實作（baseline）的數值比較。

**Acceptance Scenarios**:

1. **Given** 合法解已產生，**When** 以計分器計算加權 HPWL，**Then** 系統回傳具體數值且所有 net 均可正確計算
2. **Given** 同一輸入，**When** 比較系統解與參考實作的 HPWL，**Then** 系統解的 HPWL 不顯著劣於參考實作（作為品質基準）

---

### User Story 4 - 批次處理多個測試案例 (Priority: P3)

使用者希望對 `benchmark/testcase/` 目錄下的所有 `.txt` 測試案例逐一執行，並彙整每個案例的執行結果與最佳化指標。

**Why this priority**: 單一案例驗證後，批次測試確保系統對不同輸入都有穩定表現。

**Independent Test**: 對全部 5 個 testcase（sample, public1–4）執行並產生對應的 `.floorplan` 輸出，再以計分器批次計分。

**Acceptance Scenarios**:

1. **Given** `benchmark/testcase/` 下有 5 個 `.txt` 測試案例，**When** 系統逐一執行，**Then** 產生 5 個對應的 `.floorplan` 輸出檔案
2. **Given** 所有輸出檔案已產生，**When** 以 `scorer/score.py 002` 批次計分，**Then** 產生包含每個案例之 HPWL、合法性、verifier 結果的彙整報表

---

### Edge Cases

- 當某軟模組的面積極小（如 1）時，能否找到滿足長寬比的尺寸？
- 當硬模組佔滿大部分空間時，軟模組是否有足夠區域可放置？
- 當某 net 連接的模組為硬模組（不出現在輸出中）時，如何正確計算其中心座標？
- 當輸入檔案包含大量模組（如 public4）時，系統是否能在合理時間內完成？
- 若輸入中某 net 引用的模組名稱不存在時，系統應如何處理？

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: 系統 MUST 讀取 `.txt` 格式的輸入檔案，解析出晶片輪廓尺寸 `ChipSize W H`
- **FR-002**: 系統 MUST 解析所有軟模組 `SoftModule <name> <area>` 並記錄其名稱與最小面積
- **FR-003**: 系統 MUST 解析所有硬模組 `FixedModule <name> <x> <y> <w> <h>` 並確保輸出時其位置與尺寸與輸入完全一致
- **FR-004**: 系統 MUST 解析所有網路連接 `Net <moduleA> <moduleB> <weight>` 並使用其參與加權 HPWL 計算
- **FR-005**: 系統 MUST 為每個軟模組分配整數的 `(x, y, w, h)` 座標與尺寸
- **FR-006**: 系統 MUST 確保每個軟模組的面積 `w × h` 大於或等於其輸入指定的最小面積
- **FR-007**: 系統 MUST 確保每個軟模組的長寬比 `h/w` 落在 `[0.5, 2]` 範圍內
- **FR-008**: 系統 MUST 確保所有軟模組的左下角座標 `(x, y)` 滿足 `x ≥ 0`、`y ≥ 0`，且右上角 `(x+w, y+h)` 不超出晶片輪廓 `(W, H)`
- **FR-009**: 系統 MUST 確保所有模組（軟 + 硬）之間互不重疊（以矩形相交判定）
- **FR-010**: 系統 MUST 輸出 `.floorplan` 格式檔案，首行為 `Wirelength <value>`（可選），接著為 `NumSoftModules <n>` 及每行 `<name> <x> <y> <w> <h>`
- **FR-011**: 系統 MUST 在處理完成後以加權 HPWL 計算實際線長值，其中模組中心座標為 `(⌊x + w/2⌋, ⌊y + h/2⌋)`（向下取整），HPWL 公式為 `Σ weight × (|cx1-cx2| + |cy1-cy2|)`
- **FR-012**: 系統 MUST 對 `benchmark/testcase/` 下的所有測試案例（`sample.txt`, `public1.txt`–`public4.txt`）都能產生合法解

### Key Entities

- **晶片輪廓 (ChipSize)**: 定義整體設計區域的矩形邊界 `(W, H)`，所有模組必須在內
- **軟模組 (SoftModule)**: 面積固定但尺寸可變形的矩形模組，需分配 `(x, y, w, h)`，受長寬比與面積下限約束
- **硬模組 (FixedModule)**: 位置、尺寸完全固定的矩形模組，輸出時不需列出但會影響放置空間與 HPWL 計算
- **網路連接 (Net)**: 兩個模組之間的連線關係，附帶權重值，用於計算加權 HPWL
- **輸出解 (Floorplan)**: 所有軟模組最終分配的位置與尺寸集合，並含計算出的 HPWL 值

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 對全部 5 個測試案例（sample + public1–4）產生的解均通過合法性檢查（verifier 回傳 `[Success]` 或計分器判定 `valid=True`）
- **SC-002**: 每個測試案例的加權 HPWL 可被計分器獨立計算並回傳數值（非 null）
- **SC-003**: 在 `sample.txt` 案例上，系統產出的加權 HPWL 不超過 215（參考計分器已驗證的基準值）
- **SC-004**: 在 `public1.txt`–`public4.txt` 案例上，系統產出的加權 HPWL 不顯著劣於參考實作（baseline）之數值（差距在 20% 以內）
- **SC-005**: 單一測試案例的處理時間在 10 分鐘以內
- **SC-006**: 計分器批次計分後產生 Markdown / CSV 報表，包含所有案例之 HPWL、合法性狀態、與 verifier 結果

## Assumptions

- 輸入檔案格式嚴格遵循 `problems/002-floorplanning/reference/spec.pdf` 所定義的格式
- 測試案例均為合法輸入（模組名稱不衝突、net 引用的模組確實存在）
- 模組座標與尺寸均為整數
- 參考實作（baseline）位於 `problems/002-floorplanning/reference/src/`，可用以比較品質
- 計分器 `scorer/score.py` 為主要驗收工具，其內部合法性檢查涵蓋所有 verifier 規則
- 官方 `benchmark/verifier/verify` 為 Linux ELF，在純 Windows 環境無法直接執行，以計分器內建檢查替代
- 本實作獨立放在 `problems/002-floorplanning/experiments/<model>/` 目錄中，不與參考實作或其他模型實作混用
- 根據憲章原則，本規格為所有模型實作的共同基準，不得因模型而放寬
