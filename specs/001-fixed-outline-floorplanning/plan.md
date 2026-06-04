# Implementation Plan: 固定輪廓平面規劃 (Fixed-outline Floorplanning)

**Branch**: `001-fixed-outline-floorplanning` | **Date**: 2026-06-04 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/001-fixed-outline-floorplanning/spec.md`

## Summary

在固定晶片輪廓內放置可變形軟模組與固定硬模組，使所有模組不重疊、滿足面積與長寬比限制，並最小化加權 HPWL（Half-Perimeter Wire Length）。使用 C++11 實作，以參考實作的模擬退火 (Simulated Annealing) 為基礎架構，但強化初始放置、鄰域操作、與溫度調度策略以提升解的品質。

## Technical Context

**Language/Version**: C++11（`g++ -std=c++11 -O3` 編譯）

**Primary Dependencies**: 無外部依賴，僅使用 C++ 標準函式庫（`<iostream>`, `<fstream>`, `<vector>`, `<string>`, `<algorithm>`, `<cmath>`, `<unordered_map>`, `<random>`）

**Storage**: N/A（純計算，無需資料庫）

**Testing**: 以 `scorer/score.py 002` 計分器作為主要驗收工具，配合官方 `verifier/verify`（Linux）與手動驗證 `sample.txt` 的 HPWL ≤ 215

**Target Platform**: Windows（開發）/ Linux（提交），需跨平台可編譯

**Project Type**: CLI 程式，`./hw3 <input.txt> <output.floorplan>`

**Performance Goals**: 單一 testcase 執行時間 ≤ 10 分鐘；對 large case (public1–4) 亦能產生合法解

**Constraints**: 整數座標與尺寸；長寬比 `[0.5, 2]`；所有模組在晶片輪廓內；模組不重疊；HPWL 中心計算為 `⌊x + w/2⌋`

**Scale/Scope**: 最多 5 個 testcase，模組數從 2 到數百不等；參考實作使用 580 秒 timeout

## Constitution Check

| 憲章原則 | 狀態 | 說明 |
|---|---|---|
| I. 規格先行 | ✅ | 已有 spec.md，本 plan.md 為第二步 |
| II. 驗證器即真理 | ✅ | 驗收以 scorer/score.py 與 verifier 為準 |
| III. 公平比較 | ✅ | 實作放在 `experiments/<model>/`，獨立於參考實作 |
| IV. 可重現 | ✅ | 固定 RNG seed、記錄參數與執行結果 |
| V. 量化最佳化品質 | ✅ | 記錄每 testcase 的 HPWL 數值 |

## Project Structure

### Documentation (this feature)

```text
specs/001-fixed-outline-floorplanning/
├── spec.md              # Feature specification
├── plan.md              # This file
├── research.md          # Phase 0: algorithm research
├── data-model.md        # Phase 1: data structures
├── quickstart.md        # Phase 1: build & run guide
├── contracts/
│   └── io-format.md     # Input/output format specification
└── tasks.md             # Phase 2 output (created by /speckit.tasks)
```

### Source Code (repository root)

```text
problems/002-floorplanning/experiments/qwen/
├── src/
│   ├── main.cpp          # 程式進入點，命令列解析與流程控制
│   ├── parser.cpp        # 輸入檔案解析（ChipSize / SoftModule / FixedModule / Net）
│   ├── parser.h
│   ├── module.h          # 模組資料結構（軟/硬模組、shape 清單）
│   ├── grid.h            # 格狀空間管理（放置 / 移除 / 碰撞檢測）
│   ├── placement.cpp     # 初始放置演算法（heuristics-based）
│   ├── placement.h
│   ├── optimizer.cpp     # 模擬退火最佳化（swap / move / reshape）
│   ├── optimizer.h
│   ├── hpwl.cpp          # HPWL 計算與成本函數
│   ├── hpwl.h
│   ├── output.cpp        # .floorplan 輸出
│   ├── output.h
│   └── Makefile          # 編譯指令
├── out/                  # 執行輸出目錄（.floorplan 檔案）
└── RESULT.md             # 評測紀錄
```

**Structure Decision**: 採用多檔案 C++ 專案結構，將參考實作的單一 `main.cpp` 重構為模組化架構。分離 parser、grid、placement、optimizer、hpwl 各層，以利除錯、測試與後續改良。實作獨立放在 `experiments/qwen/`，不與參考實作混用。

## Phase 0: Research

- 分析參考實作 `reference/src/main.cpp` 的演算法架構
- 確認輸入/輸出格式細節與計分器 `scorer/lib/floorplanning.py` 的驗證規則
- 評估模擬退火參數（溫度、衰減率、迭代次數）對解品質的影響
- 記錄 research.md

## Phase 1: Design

- 定義資料模型（module、grid、net、solution）
- 設計初始放置策略（按面積排序 + 貪婪放置 + 緊密化）
- 設計鄰域操作（swap / move / reshape）
- 設計成本函數與溫度調度
- 記錄 data-model.md、quickstart.md、contracts/io-format.md

## Phase 2: Tasks

- 執行 `/speckit.tasks` 產生具體開發任務清單

## Phase 3: Implementation

- 執行 `/speckit.implement` 按任務清單實作

## Phase 4: Verification

- 對全部 5 個 testcase 執行並驗證
- 以 `scorer/score.py 002` 批次計分
- 記錄 RESULT.md
