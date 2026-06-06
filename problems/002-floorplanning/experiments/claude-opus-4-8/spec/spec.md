# Feature Specification: Fixed-outline Floorplanning Optimizer

**Feature Branch**: `002-fixed-outline-floorplanning`

**Created**: 2026-06-05

**Status**: Draft

**Input**: 固定輪廓平面規劃 (Fixed-outline Floorplanning)。在固定的晶片輪廓內擺放 soft 模組（面積給定、長寬可變形）與 fixed 模組（位置尺寸固定），所有模組不重疊且完全落在輪廓內，最小化加權線長 HPWL。完整題目見 `problems/002-floorplanning/reference/spec.pdf`。

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 — 產生合法平面規劃 (Priority: P1)

研究人員以命令列執行程式，給定一份測資 `.txt` 與輸出路徑，程式產生一份合法的 `.floorplan`：
所有 soft module 完全落在晶片輪廓 `[0,W]×[0,H]` 內、彼此及與 fixed module 皆不重疊，
fixed module 維持輸入給定的座標與尺寸不被更動，每個 soft module 的面積保持等於輸入給定值。

**Why this priority**: 合法性是計分前提，不合法即零分；所有 HPWL 與時間品質都建立在「先合法」之上。

**Independent Test**: 以 `benchmark/verifier/verify <input.txt> <output.floorplan>` 執行，輸出 `[Success]` 即視為通過，無需任何其他指標。

**Acceptance Scenarios**:

1. **Given** sample.txt（2 soft、2 fixed、3 nets），**When** 執行程式，**Then** 輸出 `.floorplan` 通過 verifier 顯示 `[Success]`。
2. **Given** 任一 public testcase，**When** 執行程式，**Then** 所有模組座標滿足 `x≥0`、`y≥0`、`x+w≤W`、`y+h≤H`，且任意兩模組矩形交集面積為 0。
3. **Given** 含 fixed module 的測資，**When** 執行程式，**Then** 輸出中各 fixed module 的 `(x,y,w,h)` 與輸入完全相同。
4. **Given** 任一 soft module，**When** 執行程式，**Then** 輸出尺寸滿足 `w×h = area`（浮點精度內），長寬比由程式自選。

---

### User Story 2 — HPWL 低於 Min 門檻 (Priority: P2)

研究人員在程式通過 verifier 後，以 `scorer/score.py` 計算加權 HPWL，期望各 public testcase 的結果
≤ 對應 **Min 門檻值**（要超越的已知最佳結果），至少優於 Reference 人類參考解。

**Why this priority**: 超越 Min 是本實驗的核心量化指標，決定模型品質排名（憲章原則 V、VI）。

**Independent Test**: 先確認 verifier `[Success]`，再以 `scorer/score.py <input> <output>` 取得 HPWL，對照下表 Min / Reference。

| testcase | **目標 Min（≤ 此值）** | Reference 參考解 | Reference runtime | Max（零分門檻） |
|----------|----------------------:|------------------:|------------------:|----------------:|
| public1  | 161,609,972           | 239,984,392       | 581.41s           | 349,768,634     |
| public2  | 20,966,863            | 38,494,434        | 45.86s            | 41,569,628      |
| public3  | 1,856,276             | 2,621,582         | 111.55s           | 5,045,921       |
| public4  | 63,024,850            | 137,686,350       | 285.35s           | 201,625,050     |

**Acceptance Scenarios**:

1. **Given** public3.txt（較小規模），**When** 執行程式，**Then** 加權 HPWL ≤ 1,856,276 且通過 verifier。
2. **Given** public2.txt，**When** 執行程式，**Then** 加權 HPWL ≤ 20,966,863 且通過 verifier。
3. **Given** 任一 public testcase，**When** 執行程式，**Then** HPWL 至少低於 Reference 值（超越人類參考解），且必定低於 Max 門檻（避免零分）。

---

### User Story 3 — 在時間上限內完成 (Priority: P3)

研究人員對全部 testcase 計時執行，每份測資須在 600 秒 wall-clock 上限內輸出合法結果。

**Why this priority**: 超時即不計分，但時間效率對結果的影響次於合法性與 HPWL 品質。

**Independent Test**: 以 wall clock 單獨對每份 public testcase 計時，記錄完成秒數，須 ≤ 600s。

**Acceptance Scenarios**:

1. **Given** public1.txt（最大測資，Reference runtime 581.41s），**When** 執行程式，**Then** 在 600 秒內完成並輸出合法結果。
2. **Given** public2/3/4.txt，**When** 執行程式，**Then** 各自在 600 秒內完成並輸出合法結果。

---

### Edge Cases

- 某 soft module 面積極大、難以在剩餘空間擺下時，程式須仍輸出合法結果，不得崩潰或超時。
- Fixed module 佔用大量晶片面積、剩餘可用空間受限（高密度）時，soft module 擺放須仍能避免重疊與越界。
- Net 兩端皆為 fixed module 時，其 HPWL 貢獻為定值，計算須仍正確。
- 最佳長寬比可能為非整數，實作須支援浮點尺寸或足以通過 verifier 的精度近似。
- 輸入含 0 條 net 時，程式須仍合法輸出（僅需滿足不重疊、不越界，HPWL 為 0）。
- Net 引用的模組名稱必為已宣告的 soft 或 fixed module（題目保證），無需處理未知名稱。

---

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: 程式 MUST 正確解析輸入 `.txt`，支援 `ChipSize W H`、`NumSoftModules`、`SoftModule <name> <area>`、`NumFixedModules`、`FixedModule <name> <x> <y> <w> <h>`、`NumNets`、`Net <A> <B> <weight>` 各關鍵字與欄位。
- **FR-002**: 程式 MUST 確保每個 soft module 的座標 `(x,y,w,h)` 滿足 `x≥0`、`y≥0`、`x+w≤W`、`y+h≤H`（完全落在固定晶片輪廓內）。
- **FR-003**: 程式 MUST 確保任意兩模組（soft–soft、soft–fixed、fixed–fixed）的矩形不重疊，即交集面積為 0。
- **FR-004**: 程式 MUST 保持每個 soft module 的面積不變（`w×h = area`，浮點精度內）；其長寬比可自由選擇以利最佳化。
- **FR-005**: 程式 MUST 保持所有 fixed module 的座標與尺寸與輸入完全相同，不得移動或縮放。
- **FR-006**: 程式 MUST 最小化加權 HPWL = Σ_net weight × (max_x − min_x + max_y − min_y)，其中極值取自該 net 所連各模組的代表點（座標定義以官方 verifier / scorer 為準）。
- **FR-007**: 程式 MUST 輸出 `.floorplan`，內容為每個模組（soft + fixed）的最終 `(x,y,w,h)`，格式與 verifier 要求相符。
- **FR-008**: 程式 MUST 在 600 秒 wall-clock 內完成每份 testcase 並輸出合法結果。
- **FR-009**: 程式 MUST 以無互動的批次模式執行，介面為命令列引數 `<input.txt> <output.floorplan>`。
- **FR-010**: 程式 MUST 對所有 5 份測資（sample + public1~4）以相同二進位檔與相同編譯設定執行，無需逐測資修改程式碼或參數檔。

### Key Entities

- **Chip**: 固定邊界矩形，寬 W、高 H；所有模組必須完全落在 `[0,W]×[0,H]` 範圍內。
- **SoftModule**: 名稱、固定面積 area、可變寬高（`w×h = area`）；擺放位置與長寬比由程式決定。
- **FixedModule**: 名稱、固定座標 `(x,y)` 與尺寸 `(w,h)`；程式不得更動。
- **Net**: 連接的模組端點（A、B）與正權重 weight；HPWL 以 weight 加權該 net 的線長貢獻。
- **Floorplan**: 所有模組最終 `(x,y,w,h)` 的集合，即程式輸出。

---

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001（合法性）**: 對 sample + public1~4 全部 5 份測資，`benchmark/verifier/verify` 均回傳 `[Success]`，合法率 100%。
- **SC-002（超越 Min）**: public1~4 各 testcase 的加權 HPWL ≤ 對應 Min 門檻（見 User Story 2 表格），達成 4 份全部超越已知最佳值。
- **SC-003（超越 Reference 下限）**: 若未能全部達 Min，各 testcase 的 HPWL 至少 < Reference 值且 < Max 門檻，即優於人類參考解且不零分。
- **SC-004（時間）**: 所有 testcase 執行時間 ≤ 600 秒；其中 public2/3/4 目標 ≤ 120 秒，保留最佳化迭代空間。
- **SC-005（可重現）**: 在相同硬體與編譯指令（`g++ -std=c++20 -O3`）下，重複執行同一 testcase 的 HPWL 差異 ≤ 5%；若使用隨機演算法須記錄 seed。

---

## Assumptions

- 程式以 C++20（`g++ -std=c++20 -O3`）編譯，在 Windows 原生環境與 Linux 均可編譯執行。
- 輸入檔格式保證合法，無需處理格式錯誤、缺漏欄位或未知模組名稱。
- Fixed modules 本身互不重疊且皆落在輪廓內（題目保證），程式無需驗證輸入合法性。
- HPWL 的代表點與精確計算方式以官方 verifier / `scorer/` 為準；本規格的 FR-006 為其文字描述。
- Soft module 的 w、h 可為浮點數，輸出精度以能通過 verifier 為準；長寬比上下限（若有）以 spec.pdf 為準。
- Runtime 600s 上限適用於單一測資的求解；scorer / verifier 的執行時間不計入此上限。
- 僅需命令列批次執行，無需圖形化輸出或互動介面。
