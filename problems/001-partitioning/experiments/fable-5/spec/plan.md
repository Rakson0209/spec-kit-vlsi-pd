# Implementation Plan: Multi-Technology Die Partitioning

**Date**: 2026-06-07 | **Spec**: [spec.md](./spec.md)

## Technical Context

**Language/Version**: C++20
**Compiler**: `g++ -std=c++20 -O3 -fopenmp -pthread`（專案內建 portable mingw64 g++ 16.1.0；先 `. .\tools\mingw64\setup-env.ps1` 載入）
**Primary Dependencies**: C++ 標準庫 + **OpenMP**（`-fopenmp`）+ `std::thread`/pthread（`-pthread`）。**不使用 Boost**（環境缺 Boost；見 constitution v0.6.0 R2 與 memory `boost-unavailable`）。
**Platform**: Windows native（mingw64）
**Scale**: `sample`(8 cells) → `public6`(~4.15M 行；估 ~1–2M cells/nets，數百萬 pins)。runtime 上限 ~300s（內部 deadline 設 ~285s 留邊際）。
**Determinism**: 同輸入重跑須等價或更好（FR-013）；multi-start 以固定 seed 序列（seed = 起點索引）保證最佳解選取可重現。
**Unknowns**: 無 `NEEDS CLARIFICATION` — spec 的合法性與指標由 `scorer/lib/partitioning.py` 完全決定（R6）。

> ⚠️ **模板修正**：`plan-template.md` 仍含過時的 Boost 參照（`-I tools/boost`、`boost::graph/geometry`、R2「Boost + 平行」）。本檔以**已批准的 constitution v0.6.0**（R2：OpenMP + pthread，**無 Boost**）為準，並以此覆寫模板的 Technical Context 與 Constitution Check。

## Approach Summary

題型 = **超圖二分割（hypergraph bipartitioning）最小割**，附**雙邊面積容量限制**，且同一 cell 的重量依所在 die 的 Tech 而異（`area_A` vs `area_B`）。採 **multilevel FM + 平行 multi-start** 的分層策略，對應 spec 三條 user story：

- **L1（US1 / P1，MVP「合法」）**：快速 parser → **可行初始分割**（每個 cell 放到相對較省的 die，受該 die 的 cap 約束）→ 立即可輸出**合法** `.out`，過 scorer。
- **L2（US2 / P2，「品質」）**：**面積受限 Fiduccia–Mattheyses (FM)** 精修 —— bucket-gain 結構、net 的 `F/T` 計數增量更新、pass 內記錄最佳前綴並回滾（roll back to best prefix）。
- **L3（US2+US3 / P2+P3，「打敗 Min + 大規模」）**：**平行 multi-start**（OpenMP / `std::thread` 多 seed 並行，取最佳合法解）；大型 case 加 **multilevel**（coarsen → 粗階初分割 → uncoarsen + 各階 FM 精修），逼近 hMETIS/KaHyPar 級割值並維持近線性時間。

完整決策見 [research.md](./research.md)；資料結構見 [data-model.md](./data-model.md)；介面契約見 [contracts/](./contracts/)；驗證步驟見 [quickstart.md](./quickstart.md)。

## Constitution Check (v0.6.0)

| 守則 | 對應設計 | 狀態 |
|------|----------|:---:|
| **R1** Baseline Fallback | 先自寫 multi-start FM → 全 case 跑 scorer；若**全部** case 都比 `reference/src/` 差 → 將 reference 移植成 std（去 Boost）放入 `experiments/<model>/` 再優化。只要任一 case 優於 reference 即續用自寫。見 research §10 | ✅ planned |
| **R2** 平行優先（OpenMP+pthread，**無 Boost**） | multi-start 並行、平行 cost/gain、平行 coarsening；旗標 `-fopenmp -pthread`；零 Boost 依賴 | ✅ planned |
| **R3** 合法性硬門檻 | 可行初分割保證起點合法；FM/multilevel 任何移動以「**目的 die 用率 ≤ cap**」為門檻；終局必輸出合法解 | ✅ planned |
| **R4** 全 case 守門 | quickstart 提供一鍵對 7 個 testcase 計分；每次改 code 後全跑 | ✅ planned |
| **R5** 規格先行 | spec → plan → tasks → code；本檔為 plan 階段，未寫實作碼 | ✅ |
| **R6** 評測即真理 | 合法性與割值一律以 `scorer/lib/partitioning.py` 為準；FM 可行性用 `≤ cap`（含 1e-9 容差）對齊 scorer，而非 reference 的嚴格 `<` | ✅ |

**Gate 結果**：無違規 → Complexity Tracking 留空。

## Project Structure

Phase 2 各受測模型在自身目錄產生實作（單一翻譯單元，便於各模型獨立比較）：

```
problems/001-partitioning/experiments/<model>/
├── main.cpp        # parser + 初分割 + FM + multilevel + multi-start + writer
├── Makefile        # g++ -std=c++20 -O3 -fopenmp -pthread -o hw2 main.cpp
├── spec/           # 由 opus spec/ 複製（含本 plan.md / research.md / tasks.md）
└── out/            # <case>.out（sample, public1..6）
```

> 規劃端（本目錄 `experiments/claude-opus-4-8/spec/`）只產 SDD 文件；opus 自身實作放 `experiments/claude-opus-4-8/main.cpp`。

## Phase Outputs

- **Phase 0 → [research.md](./research.md)**：演算法分類、FM gain 推導與 `≤cap` 語意、初分割、multi-start 平行、multilevel、gain 桶結構、I/O 與資料布局、時間預算/決定論、R1 fallback 移植細節、目標差距分析。
- **Phase 1 → 設計產物**：[data-model.md](./data-model.md)、[contracts/cli-contract.md](./contracts/cli-contract.md)、[contracts/io-format.md](./contracts/io-format.md)、[quickstart.md](./quickstart.md)。
- **Phase 2（`/speckit.tasks`）**：依本 plan 產生 `tasks.md`（依賴排序的可執行任務）。

## Complexity Tracking

> 無 Constitution 違規，本節留空。

| Violation | Why | Rejected Alternative |
|-----------|-----|----------------------|
| —         | —   | —                    |
