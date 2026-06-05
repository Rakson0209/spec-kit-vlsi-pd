# Implementation Plan: 固定輪廓平面規劃 (Fixed-outline Floorplanning)

**Branch**: `002-floorplanning` | **Date**: 2026-06-05 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `problems/002-floorplanning/experiments/qwen3.6-27b-autoround/spec/spec.md`

## Summary

在固定晶片輪廓內放置可變形的軟模組（area 固定、aspect ratio ∈ [0.5, 2]）和位置固定的硬模組，滿足無重疊約束後，最小化加權 HPWL。採用 **Sequence-Pair + Simulated Annealing** 方案，以超越各 testcase 的 Min 門檻為目標。

## Technical Context

**Language/Version**: C++11（憲章編譯基準 `g++ -std=c++11 -O3`）

**Primary Dependencies**: 無外部依賴（標準程式庫 only：vector, map, algorithm, fstream, cmath, random）

**Storage**: 無（pure computation CLI tool，讀 .txt → 輸出 .floorplan）

**Testing**: 以 `scorer/score.py` 做合法性檢查與 HPWL 計算；以 `benchmark/verifier/verify`（Linux binary）做最終驗證

**Target Platform**: Linux（官方 verifier 為 Linux ELF）；開發可在 Windows（以 Python scorer 代做合法性檢查）

**Project Type**: CLI tool（`./hw3 <input.txt> <output.floorplan>`）

**Performance Goals**: 每筆 testcase ≤ 600s；public1（最大 testcase）目標 ≤ 300s

**Constraints**: 
- 模組座標/尺寸為整數
- 中心座標 `x + w//2`（整數除法向下取整）
- 長寬比 h/w ∈ [0.5, 2.0]
- 所有模組不重疊且在輪廓內
- 硬模組不可移動

**Scale/Scope**: 
- 最多 28 軟模組 + 14 硬模組 + 108 nets（public4）
- 最大晶片 11267×10450（public1, ~1.18 億 pixels）
- 5 筆 testcase（sample + public1~4）

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

基於 `.specify/memory/constitution.md` 憲章，以下守則必須在計畫階段通過：

| # | 守則 | 驗證項目 | 狀態 |
|---|------|---------|------|
| I | **規格先行** | spec.md 存在且涵蓋輸入/輸出格式、限制條件、最佳化目標、驗收標準 | ✅ Pass |
| II | **驗證器即真理** | 以 `benchmark/verifier/verify` 為正確性依據；開發階段以 `scorer/score.py` 做 interim 檢查 | ✅ Pass |
| III | **公平比較** | 實作獨立放在 `experiments/qwen3.6-27b-autoround/`，不與其他模型共用中間產物 | ✅ Pass |
| IV | **可重現** | 記錄模型版本、SDD 產物、編譯/執行指令、測資、verifier 結果、最佳化數值、時間 | ✅ Pass |
| V | **量化最佳化品質** | 明確最佳化目標 = 加權 HPWL；SC-002~SC-005 各附 Min 門檻值 | ✅ Pass |
| VI | **研究先行，超越基準** | `research.md` 已：(1) 記錄 baseline Min；(2) 調研 5 種候選演算法；(3) 選定 Sequence-Pair + SA | ✅ Pass |

**編譯基準**: `g++ -std=c++11 -O3`
**評測三項數據**: 通過/失敗、加權 HPWL、執行時間，缺一不可。

**最佳化優先級**:
1. **結果品質優先** — 以「達到並超越 Min 門檻」為第一優先
2. **執行效率次之** — 品質相近時選更快的方案

## Project Structure

### Documentation (this feature)

```text
problems/002-floorplanning/experiments/qwen3.6-27b-autoround/spec/
├── spec.md              # Feature specification
├── plan.md              # This file
├── research.md          # Phase 0: algorithm research & selection
├── data-model.md        # Phase 1: entity definitions
├── quickstart.md        # Phase 1: validation guide
├── contracts/           # Phase 1: I/O format contracts
│   ├── input.txt.md
│   └── output.floorplan.md
└── tasks.md             # Phase 2: implementation tasks (created by /speckit.tasks)
```

### Source Code

```text
problems/002-floorplanning/experiments/qwen3.6-27b-autoround/
├── src/
│   ├── main.cpp         # Entry point: parse CLI args, orchestrate pipeline
│   ├── parser.cpp/.h    # Parse .txt input → internal data structures
│   ├── module.cpp/.h    # Module class (soft/fixed), area, aspect ratio
│   ├── net.cpp/.h       # Net class, weighted connections
│   ├── seqpair.cpp/.h   # Sequence-pair encoding + constraint graph decoding
│   ├── evaluator.cpp/.h # HPWL calculation, validity checks
│   ├── sa.cpp/.h        # Simulated annealing engine, neighbor operations
│   └── placer.cpp/.h    # Net-aware initial placement heuristic
├── Makefile             # g++ -std=c++11 -O3 build
├── output/              # Generated .floorplan files
└── RESULT.md            # Test results per Constitution IV
```

**Structure Decision**: 單一 CLI project。src/ 下按職責拆成 7 個 module（parser, module, net, seqpair, evaluator, sa, placer），符合 C++ 專案常規。

## Complexity Tracking

> 本次憲章檢查全部通過，無需要變通的守則違規。

## Risk & Mitigation

| 風險 | 影響 | 緩解 |
|------|------|------|
| Sequence-Pair 編碼/解碼實作複雜 | 開發時間增加 | 先實作最小可行版本驗證核心邏輯 |
| SA 參數需調校 | 可能無法一次達 Min | 先以 public2/public3 驗證算法正確性 |
| 大 testcase 運行時間接近 600s | 可能逾時 | 動態 SA 參數、早期終止 |
| public1/public4 Min 門檻與 Reference 差距 >50% | 可能無法超越 | 備用: clustering 預處理 + multi-start SA |
