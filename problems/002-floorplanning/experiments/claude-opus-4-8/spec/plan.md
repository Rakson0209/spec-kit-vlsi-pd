# Implementation Plan: Fixed-outline Floorplanning Optimizer

**Branch**: `main`（feature dir：`experiments/claude-opus-4-8/`）| **Date**: 2026-06-05 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `problems/002-floorplanning/experiments/claude-opus-4-8/spec/spec.md`

## Summary

在固定晶片輪廓內擺放 soft / fixed 模組，最小化加權 HPWL（2-pin nets、以模組中心計）。
**技術路線（第一性原理）**：本題模組數極小（soft 15–28），HPWL 只取決於模組「中心相對位置」，
因此把問題還原為 **以線長為唯一目標的擺放問題**，而非傳統面積導向的 floorplan 壓縮問題。
採用 **力導向 / 解析式全域擺放（直接最小化加權線長）→ 形狀與長寬比指派 → 貪婪合法化 → HPWL-only 模擬退火精修**，
退火階段以 **無網格（grid-free）O(N²) 矩形重疊檢查** 取代 baseline 的全晶片布林網格，使單次迭代成本從
O(晶片面積) 降到 O(N²)（N≤28，≈784 次比較），在 600s 內可跑數百萬次迭代。以此**超越** baseline 的
Min 門檻（161.6M / 21.0M / 1.86M / 63.0M）。詳見 [research.md](./research.md)。

## Technical Context

**Language/Version**: C++11（`g++ -std=c++11 -O3`）

**Primary Dependencies**: 僅 C++ 標準函式庫（無第三方）；不連結外部 lib（003 才需 reference/obj）

**Storage**: 純檔案 I/O — 讀 `.txt` 測資、寫 `.floorplan`；無資料庫

**Testing**: 官方 `benchmark/verifier/verify`（Linux）為正確性真理；`scorer/score.py`（Windows）計分；
sample + public1~4 共 5 份測資為整合測試

**Target Platform**: Windows 原生（開發/計分）與 Linux（verifier）皆須可編譯執行；單執行緒 CLI

**Project Type**: 單一 C++ CLI 程式（命令列批次：`hw3 <input.txt> <output.floorplan>`）

**Performance Goals**: 每份測資 ≤ 600s（硬上限）；public2/3/4 目標 ≤ 120s。
HPWL 目標：4 份 public 全部 ≤ Min 門檻（見 research.md 門檻表），保底 < Reference 且 < Max。

**Constraints**（取自官方 verifier 字串，為合法性真理）:
- 所有模組落在 `[0,W]×[0,H]`，整數座標（verifier 幾何用 `Rectangle(long×4)`）。
- 任意兩模組矩形不重疊（soft–soft、soft–fixed、fixed–fixed）。
- 每個 soft module：`w×h ≥ 給定 area`（min area，非等號）；長寬比 `w/h ∈ [0.5, 2.0]`。
- Fixed module 座標尺寸不可更動。
- 輸出第一行 `Wirelength <值>` 必須等於 verifier 計算的整數 HPWL（否則 `[Error] Wrong Wirelength!`）。
- 輸出只列 soft modules（含全部、不得重複或遺漏）。

**Scale/Scope**: soft 15–28、fixed 5–14、nets 39–108、晶片邊長最大約 11267×10450（public1）。

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| # | 守則 | 驗證項目 | 狀態 |
|---|------|---------|------|
| I | **規格先行** | spec.md 已涵蓋輸入/輸出格式、限制、最佳化目標、驗收標準 | ✅ |
| II | **驗證器即真理** | 以 `benchmark/verifier/verify` 為正確性依據；合法性常數取自 verifier 字串 | ✅ |
| III | **公平比較** | 全部產物獨立於 `experiments/claude-opus-4-8/`；演算法選型為本模型獨立推導，未參照他模型 SDD 產物（僅引用共用題目敘述與 `reference/` baseline） | ✅ |
| IV | **可重現** | plan/research 記錄編譯/執行/驗證/計分指令；RESULT.md 待 implement 階段填數值與 seed | ✅ |
| V | **量化最佳化品質** | 目標為加權 HPWL 數值，對照 Min/Reference/Max 門檻 | ✅ |
| VI | **研究先行，超越基準** | research.md 已：(1) 記錄 Min 門檻；(2) 調研候選演算法與取捨；(3) 選定有潛力超越 baseline 的方案 | ✅ |

**編譯基準**: `g++ -std=c++11 -O3`（本題不需連結 `reference/obj`）。
**評測三項數據**: verifier 通過/失敗、HPWL 數值、執行時間/回合數，缺一不可。

**最佳化優先級遵守說明**:
1. **結果品質優先** — 丟棄 baseline cost 中的面積項（固定輪廓下面積無關目標），cost 僅含 HPWL；
   以解析式全域擺放取得近最佳「相對位置」，直擊 Min 門檻。
2. **執行效率次之** — 無網格 O(N²) 重疊檢查取代 O(晶片面積) 網格操作；記憶體從 ~100MB 級網格降到 O(N)。

> Constitution Check 全數通過，無違規，Complexity Tracking 留空。

## Project Structure

### Documentation (this feature)

```text
problems/002-floorplanning/experiments/claude-opus-4-8/spec/
├── plan.md              # 本檔（/speckit.plan 產出）
├── research.md          # Phase 0（演算法調研與選型，超越 baseline 論證）
├── data-model.md        # Phase 1（實體、欄位、合法性規則、HPWL 定義）
├── quickstart.md        # Phase 1（build / run / verify / score 驗證指南）
├── contracts/           # Phase 1（輸入/輸出/CLI/verifier 介面契約）
│   ├── io-format.md
│   └── cli-contract.md
├── checklists/
│   └── requirements.md  # /speckit.specify 產出
└── tasks.md             # Phase 2（/speckit.tasks 產出，本指令不建立）
```

### Source Code (實作位置)

```text
problems/002-floorplanning/experiments/claude-opus-4-8/
├── main.cpp             # 單檔實作（parser → global place → legalize → SA → output）
├── Makefile             # g++ -std=c++11 -O3 -o hw3 main.cpp
├── RESULT.md            # 評測紀錄（implement 後填）
└── *.floorplan          # 各測資輸出（implement 後產生）
```

**Structure Decision**: 單一 C++ 程式、單檔 `main.cpp`（題目規模小、無模組化必要，便於 verifier 與跨模型公平比較）。
邏輯分區（同檔內函式）：輸入解析、全域擺放、合法化、SA 精修、HPWL 計算、輸出。

## Complexity Tracking

> Constitution Check 無違規，本表留空。

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| —         | —          | —                                    |
