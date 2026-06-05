# Feature Specification: 固定輪廓平面規劃 (Fixed-outline Floorplanning)

**Feature Branch**: `002-floorplanning`

**Created**: 2026-06-05

**Status**: Draft

**Input**: User description: "固定輪廓平面規劃 (Fixed-outline Floorplanning)。完整題目見 problems/002-floorplanning/reference/spec.pdf。輸入 .txt 含 ChipSize W H、NumSoftModules 與 SoftModule <name> <area>(面積給定、長寬可變形)、NumFixedModules 與 FixedModule <name> <x> <y> <w> <h>(位置尺寸固定)、NumNets 與 Net <A> <B> <weight>。需求:所有模組不重疊且完全落在固定晶片輪廓內,最小化加權線長 HPWL。輸出 .floorplan 為各模組最終座標與尺寸。測資在 benchmark/testcase/(sample + public1~4)。合法性與計分用 scorer/(官方 verifier 為 Linux binary)。目標 wirelength ≤ 各 testcase 的 Min(見本題 README 的 Baseline 指標門檻表)。"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - 讀取輸入並產生合法平面配置 (Priority: P1)

使用者提供一份包含晶片尺寸、軟模組（面積固定、可變形）、硬模組（位置尺寸固定）與 net 連線的 `.txt` 輸入檔，系統能解析該檔案並產生一份合法的 `.floorplan` 輸出檔，所有模組不重疊、全部落在晶片輪廓內、滿足長寬比與面積約束。

**Why this priority**: 這是此功能的基礎——若輸出無法通過合法性驗證，最佳化目標沒有任何意義。

**Independent Test**: 可對任意單筆 testcase 獨立執行，以 `scorer/score.py` 的內建合法性檢查或官方 `benchmark/verifier/verify` 判定是否輸出 `[Success]`。

**Acceptance Scenarios**:

1. **Given** 輸入檔案 `sample.txt`（2 個軟模組、2 個硬模組、3 條 net），**When** 執行系統並產生 `sample.floorplan`，**Then** 所有模組不重疊、全部在輪廓內、面積 >= 指定最小值、長寬比 h/w ∈ [0.5, 2]，且 verifier 回傳 `[Success]`
2. **Given** 輸入檔案 `public1.txt`（15 個軟模組、5 個硬模組、45 條 net），**When** 執行系統，**Then** 輸出的 `.floorplan` 能通過合法性檢查
3. **Given** 輸入檔案中包含與晶片邊界貼齊的硬模組（如 PAD 模組），**When** 執行系統，**Then** 軟模組能正確避開硬模組區域且不超出晶片範圍

### User Story 2 - 最小化加權線長 (Priority: P1)

使用者期望系統在滿足合法性約束的前提下，產生加權 HPWL（Half Perimeter Wirelength）最小的平面配置，使其達到的 wirelength 值不超過各 testcase 指定的 Min 門檻值。

**Why this priority**: 這是題目核心最佳化目標，也是比較不同方案品質的關鍵指標。

**Independent Test**: 透過 `scorer/score.py` 獨立重算 wirelength 並與基準 Min 值比較。

**Acceptance Scenarios**:

1. **Given** `public1.txt` 輸入，**When** 系統產生輸出，**Then** 加權 HPWL ≤ 161,609,972
2. **Given** `public2.txt` 輸入，**When** 系統產生輸出，**Then** 加權 HPWL ≤ 20,966,863
3. **Given** `public3.txt` 輸入，**When** 系統產生輸出，**Then** 加權 HPWL ≤ 1,856,276
4. **Given** `public4.txt` 輸入，**When** 系統產生輸出，**Then** 加權 HPWL ≤ 63,024,850
5. **Given** `sample.txt` 輸入，**When** 系統產生輸出，**Then** 加權 HPWL 小於或等於已知最佳值

### User Story 3 - 在運算時間限制內完成 (Priority: P2)

使用者期望系統在合理時間內完成配置（每筆 testcase 執行時間不超過 600 秒），以便在實際使用場景下具有可用性。

**Why this priority**: 即使結果合法且品質良好，若運行時間過長则不具實用價值。

**Independent Test**: 對每筆 testcase 計時，確認在 600 秒內完成。

**Acceptance Scenarios**:

1. **Given** `public1.txt`（最大 testcase），**When** 執行系統，**Then** 執行時間 ≤ 600 秒
2. **Given** 所有 5 筆 testcase，**When** 逐一執行，**Then** 每筆執行時間皆 ≤ 600 秒

### Edge Cases

- 輸入檔案中軟模組的面積很小（如 10,000），需要極細長或極扁平的比例來滿足長寬比約束
- 硬模組大量佔據晶片邊緣，使可用空間形成不規則形狀
- net 的權重極高（如 3000+），使特定模組配對的配置對總線長影響重大
- 軟模組數量多（28 個）且 net 密集（108 條），需要高效演算法避免組合爆炸
- 模組面積總和接近晶片總面積，幾乎沒有空隙可供調配

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: 系統必須能解析 `.txt` 格式的輸入檔，正確讀取 `ChipSize`（晶片尺寸）、`NumSoftModules` 及各 `SoftModule`（名稱、最小面積）、`NumFixedModules` 及各 `FixedModule`（名稱、位置、尺寸）、`NumNets` 及各 `Net`（兩個模組名稱、權重）
- **FR-002**: 系統必須為每個軟模組分配整數座標 (x, y) 與整數尺寸 (w, h)，使得 w × h ≥ 該模組指定的最小面積
- **FR-003**: 系統必須確保每個軟模組完全落在晶片輪廓內（0 ≤ x, y 且 x + w ≤ ChipW, y + h ≤ ChipH）
- **FR-004**: 系統必須確保所有模組（軟模組 + 硬模組）之間互不重疊（邊界貼齊不算重疊）
- **FR-005**: 系統必須確保每個軟模組的長寬比 h/w ∈ [0.5, 2.0]
- **FR-006**: 系統必須保持硬模組的位置與尺寸與輸入檔完全一致，不得移動或改變
- **FR-007**: 系統必須計算加權 HPWL = Σ weight × (|cx₁ - cx₂| + |cy₁ - cy₂|)，其中模組中心座標向下取整（cx = x + w//2, cy = y + h//2）
- **FR-008**: 系統必須以最小化總加權 HPWL 為最佳化目標
- **FR-009**: 系統必須以 `.floorplan` 格式輸出結果，包含每個軟模組的名稱、x、y、w、h（按輸入順序排列），且檔案首行可選包含 `Wirelength <value>`
- **FR-010**: 系統必須在每筆 testcase 執行時間不超過 600 秒內完成並輸出結果
- **FR-011**: 系統必須對所有 5 筆 testcase（sample, public1~4）均能產生合法且達標的結果

### Key Entities

- **晶片輪廓 (Chip)**: 固定的矩形區域 (ChipW × ChipH)，所有模組必須完全在此範圍內
- **軟模組 (SoftModule)**: 面積固定但長寬可自由變形的矩形模組，需滿足最小面積與長寬比約束
- **硬模組 (FixedModule)**: 位置與尺寸完全固定的矩形模組（如 PAD/IO pad），不可移動
- **net**: 兩個模組之間的連線關係，包含權重；HPWL 計算基於模組中心點
- **平面配置 (Floorplan)**: 所有模組最終的 (x, y, w, h) 座標與尺寸組合

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 所有 5 筆 testcase 的輸出均通過合法性驗證（verifier 回傳 `[Success]` 或 `scorer/score.py` 合法性檢查通過，無任何 violation）
- **SC-002**: `public1` 加權 HPWL ≤ 161,609,972（已知最佳 Min）
- **SC-003**: `public2` 加權 HPWL ≤ 20,966,863（已知最佳 Min）
- **SC-004**: `public3` 加權 HPWL ≤ 1,856,276（已知最佳 Min）
- **SC-005**: `public4` 加權 HPWL ≤ 63,024,850（已知最佳 Min）
- **SC-006**: 每筆 testcase 執行時間 ≤ 600 秒
- **SC-007**: `sample` 加權 HPWL 小於或等於參考解的已知結果

## Assumptions

- 輸入格式嚴格符合規範：`ChipSize`、`NumSoftModules`、`SoftModule`、`NumFixedModules`、`FixedModule`、`NumNets`、`Net` 的順序與格式固定
- 模組座標、尺寸均為整數（輸入與輸出皆為整數）
- 模組中心座標計算使用整數除法向下取整（`w // 2`、`h // 2`）
- 硬模組在輸入檔案中已被確定位置，不需任何調整
- 兩模組邊界相切（恰好貼齊、不重疊）是合法的，重疊判定為嚴格內部相交
- 輸入檔案中的模組名稱在 net 定義中一定會對應到某個存在的軟模組或硬模組
- 執行環境可使用 g++ 編譯器（-std=c++11 -O3）進行編譯
- 官方 `benchmark/verifier/verify` 為 Linux ELF 執行檔，Windows 環境下以 `scorer/score.py` 的內建合法性檢查作為替代
- 輸出 `.floorplan` 檔的格式需與 scorer 解析邏輯一致：首行可選 `Wirelength <value>`，接著 `NumSoftModules <n>`，然後逐行輸出 `<name> <x> <y> <w> <h>`
- 題目提供的 `reference/spec.pdf` 為完整問題敘述，但因 PDF 無法在本機解讀，以上規格基於 README、scorer 程式碼與 testcase 分析得出
