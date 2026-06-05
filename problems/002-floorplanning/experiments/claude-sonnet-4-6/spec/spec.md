# Feature Specification: Fixed-outline Floorplanning Optimizer

**Feature Branch**: `002-fixed-outline-floorplanning`

**Created**: 2026-06-05

**Status**: Draft

**Input**: 固定輪廓平面規劃 (Fixed-outline Floorplanning)。在固定晶片輪廓內擺放 soft/fixed 模組，最小化加權線長 HPWL。

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Valid Floorplan Generation (Priority: P1)

研究人員執行程式，輸入一份測資 `.txt`，程式輸出合法的 `.floorplan` 檔案：
所有 soft modules 落在晶片輪廓內且與其他模組不重疊；fixed modules 維持原始座標不移動。

**Why this priority**: 不合法即零分，合法性是所有後續品質衡量的前提。

**Independent Test**: 以 `benchmark/verifier/verify <input> <output>` 執行，顯示 `[Success]` 即視為通過。

**Acceptance Scenarios**:

1. **Given** sample.txt（2 soft, 2 fixed, 3 nets），**When** 執行程式，**Then** 輸出 `.floorplan` 通過 verifier，顯示 `[Success]`。
2. **Given** public1.txt（大型測資），**When** 執行程式，**Then** 輸出 `.floorplan` 通過 verifier，所有座標均在 `[0, W] × [0, H]` 內，任意兩模組矩形不重疊。
3. **Given** 測資含 fixed modules，**When** 執行程式，**Then** fixed modules 座標與尺寸與輸入完全相同，不得被移動或縮放。

---

### User Story 2 — HPWL Minimization Below Min Threshold (Priority: P2)

研究人員執行程式後，使用 `scorer/score.py` 計算加權 HPWL，期望結果 ≤ 各 testcase 的 **Min 門檻值**（見下表）。

**Why this priority**: 超越 Min 是本實驗的核心量化指標，決定模型排名。

**Independent Test**: 先確認通過 verifier，再以 `scorer/score.py` 取得 HPWL 數值，對比 Min 門檻。

| testcase | Min 門檻（目標 ≤） | Reference 參考解 |
|----------|------------------:|------------------:|
| public1  | 161,609,972       | 239,984,392       |
| public2  | 20,966,863        | 38,494,434        |
| public3  | 1,856,276         | 2,621,582         |
| public4  | 63,024,850        | 137,686,350       |

**Acceptance Scenarios**:

1. **Given** public3.txt（較小規模），**When** 執行程式，**Then** HPWL ≤ 1,856,276 且通過 verifier。
2. **Given** public2.txt，**When** 執行程式，**Then** HPWL ≤ 20,966,863 且通過 verifier。
3. **Given** 任意 public testcase，**When** 執行程式，**Then** HPWL 至少低於 Reference 值（超越人類參考解）。

---

### User Story 3 — Runtime Within 600s (Priority: P3)

研究人員對全部 testcase 計時執行，每份測資須在 600 秒內完成。

**Why this priority**: 超時即不計分，但時間效率對使用者體驗影響次於合法性與 HPWL 品質。

**Independent Test**: 以 wall clock 計時，單獨對每份 public testcase 執行，記錄完成秒數。

**Acceptance Scenarios**:

1. **Given** public1.txt（最大測資，Reference runtime 581.41s），**When** 執行程式，**Then** 在 600 秒內完成並輸出合法結果。
2. **Given** public2/3/4.txt，**When** 執行程式，**Then** 各自在 600 秒內完成。

---

### Edge Cases

- Soft module 面積過大，使其難以在剩餘空間擺下時，程式須仍能輸出合法結果（不得崩潰或超時）。
- Fixed modules 佔用大量晶片面積，剩餘空間受限時，soft module 擺放演算法須能應對高密度情境。
- Net 僅含固定模組（兩端皆為 fixed module）時，HPWL 貢獻固定，計算須仍正確。
- Soft module 的最佳長寬比（aspect ratio）可能為非整數；實作須支援浮點尺寸或合理精度的整數近似。
- 輸入含 0 條 net 時，程式須仍合法輸出（僅需不重疊、不超出邊界）。

---

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: 程式 MUST 正確解析輸入 `.txt`，支援 `ChipSize`, `NumSoftModules`, `SoftModule`, `NumFixedModules`, `FixedModule`, `NumNets`, `Net` 等關鍵字，欄位格式如問題說明。
- **FR-002**: 程式 MUST 確保所有 soft module 座標 `(x, y, w, h)` 滿足 `x ≥ 0`、`y ≥ 0`、`x + w ≤ W`、`y + h ≤ H`（chip outline 內）。
- **FR-003**: 程式 MUST 確保任意兩模組（soft 與 soft、soft 與 fixed、fixed 與 fixed）不重疊，即矩形交集面積為 0。
- **FR-004**: 程式 MUST 保持每個 soft module 的面積不變：`w × h = area`（在浮點精度範圍內）。Soft module 長寬比可自由選擇。
- **FR-005**: 程式 MUST 保持所有 fixed modules 的座標與尺寸不變。
- **FR-006**: 程式 MUST 最小化加權 HPWL = Σ（weight × (max_x − min_x + max_y − min_y)），其中 max/min 取每條 net 所含所有模組中心點的 x / y 極值。
- **FR-007**: 程式 MUST 輸出 `.floorplan` 檔案，內容為每個模組（soft + fixed）的最終 `(x, y, w, h)` 座標，格式與 verifier 要求相符。
- **FR-008**: 程式 MUST 在 600 秒 wall-clock 時間內完成每份 testcase 的執行，並輸出合法結果。
- **FR-009**: 程式 MUST 能在無任何互動的批次模式下執行（命令列引數：`<input.txt> <output.floorplan>`）。

### Key Entities

- **Chip**: 固定邊界矩形，寬 W、高 H；所有模組必須完全落在此範圍內。
- **SoftModule**: 名稱、面積（area，固定）、可變寬高（w×h = area），擺放位置與長寬比由程式決定。
- **FixedModule**: 名稱、固定座標 (x, y) 與尺寸 (w, h)，程式不得移動。
- **Net**: 兩端模組名稱 A、B，以及 weight（正整數），代表連線重要性；HPWL 乘以 weight 作為線長貢獻。
- **Floorplan**: 所有模組的最終座標與尺寸集合，即程式的輸出結果。

---

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001（合法性）**: 對 sample + public1~4 全部 5 份測資，`benchmark/verifier/verify` 均回傳 `[Success]`，合法率 100%。
- **SC-002（HPWL 超越 Min）**: public1~4 各 testcase 的加權 HPWL ≤ 對應 Min 門檻值（見 User Story 2 表格），即 4 份全部超越已知最佳值。
- **SC-003（超越 Reference）**: 若無法全部達到 Min，至少各 testcase 的 HPWL < Reference 值，即優於人類參考解。
- **SC-004（時間）**: 所有 testcase 執行時間 ≤ 600 秒；其中 public2/3/4 目標 ≤ 120 秒（留出最佳化迭代空間）。
- **SC-005（可重現）**: 在相同硬體與編譯指令（`g++ -std=c++11 -O3`）下，重複執行同一 testcase 的 HPWL 差異 ≤ 5%（若使用隨機演算法需記錄 seed）。

---

## Assumptions

- 程式以 C++11 編譯（`g++ -std=c++11 -O3`），在 Windows 原生環境與 Linux 均可編譯執行。
- 輸入檔案格式保證合法，無需處理格式錯誤或缺損欄位。
- Fixed modules 本身不互相重疊（題目保證），程式無需驗證。
- HPWL 計算使用模組的左下角座標 + 半寬/高（中心點）取極值，詳細定義以官方 verifier / scorer 為準。
- Soft module 的 w、h 可為浮點數，輸出精度以能通過 verifier 為準。
- 不設最低面積或最大長寬比限制（soft module 可為細長形）；實際測資的長寬比限制見 spec.pdf。
- 僅需支援命令列批次執行，無需圖形化輸出或互動介面。
- Runtime 600s 限制適用於單一測資；scorer 執行不計入此限制。
