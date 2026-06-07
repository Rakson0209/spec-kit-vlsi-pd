# spec-kit-vlsi-pd

以 **Spec-Driven Development (SDD)** 流程重新實作 **VLSI 實體設計自動化 (Physical Design Automation)** 的經典演算法題目，
**比較不同 LLM 模型在相同規格下的實作（coding）能力**。

---

## 為什麼做這件事（第一性原理）

VLSI 實體設計題目具備三個利於模型比較的本質特性：

1. **規格明確**：輸入/輸出格式固定、評分指標客觀（cut size、面積、HPWL…），可被機器驗證。
2. **可機器評測**：每題有客觀的合法性規則與指標，計分器可自動判定對錯，不需人工判讀。
3. **有量化品質指標**：不只「對不對」，還有「好不好」（最佳化目標），能拉出模型間的高下。

因此這是一個能**客觀量化「模型實作能力差異」**的理想實驗場。

---

## 實驗目標

- **RQ**：在相同的 SDD 規格（spec → plan → tasks）下，不同模型的 `/speckit.implement` 產出正確率與最佳化品質差異多大？

**控制變因**：spec / plan / tasks 由 `claude-opus-4-8` 統一產出（固定規格）。  
**唯一變因**：執行 `/speckit.implement` 的模型。

每個實驗以三項客觀數據對比：**是否合法（scorer 檢查）**、**最佳化指標數值**、**執行時間**。

---

## 資料夾架構

```
spec-kit-vlsi-pd/
├── .specify/                 # spec-kit 引擎（templates / scripts / workflows / constitution）
│   └── memory/constitution.md
├── .claude/                  # Claude Code skill 與 slash 命令
├── .qwen/                    # Qwen Code slash 命令（受測模型之一）
├── problems/                 # ★ 依題目分組
│   ├── 001-partitioning/
│   ├── 002-floorplanning/
│   └── 003-global-placement/
│       ├── reference/        # 人類參考解（baseline）：原始碼、spec.pdf、report.pdf
│       ├── benchmark/        # 共用測資 testcase/ + verifier/
│       └── experiments/
│           ├── claude-opus-4-8/
│           │   ├── spec/     # ★ 共用規格（Phase 1，所有模型共享）
│           │   └── main.cpp  # opus 自身實作
│           └── <model>/      # 各受測模型的實作
│               └── main.cpp
├── scorer/                   # 批次計分器（純 Python，Windows 原生可跑）
└── docs/                     # 實驗結果彙整
```

每個 `problems/NNN-*/` 內固定三個子目錄：

| 子目錄 | 用途 |
|---|---|
| `reference/` | 人類參考解（清大 CS6135 作業原始碼）+ `spec.pdf` + `report.pdf`，作為 baseline |
| `benchmark/` | `testcase/`（共用測資），所有模型共用同一份；評測用根目錄的 `scorer/` |
| `experiments/` | `claude-opus-4-8/spec/`（共用規格）+ 各模型實作目錄 |

---

## 實驗流程

### Phase 1：規劃（每題執行一次，由 claude-opus-4-8 完成）

以 `claude-opus-4-8` 對每道題跑完整 SDD 規劃鏈：

```
/speckit.specify → /speckit.plan → /speckit.tasks
```

產物存於 `experiments/claude-opus-4-8/spec/`，包含：
- `spec.md`：功能規格
- `research.md` + `plan.md`：演算法研究與技術計畫
- `tasks.md`：可執行任務清單（**所有模型共享**）

### Phase 2：實作比較（每個受測模型各自執行）

各模型依 Phase 1 的 `tasks.md` 執行：

```
/speckit.implement
```

各模型產物進各自的 `experiments/<model>/` 目錄，互不干擾。
用 `scorer/score.py` 批次計分，輸出結果存至 `docs/`。

---

## SDD 工作流程（spec-kit）

> 完整操作手冊見 [`PROMPT.md`](PROMPT.md)。

| 命令 | 作用 | 執行者 |
|---|---|---|
| `/speckit.specify` | 由題目需求產生規格 `spec.md` | claude-opus-4-8 |
| `/speckit.clarify` | （選用）澄清模糊處 | claude-opus-4-8 |
| `/speckit.plan` | 產生技術計畫 `plan.md` + `research.md` | claude-opus-4-8 |
| `/speckit.tasks` | 拆解為可執行任務 `tasks.md` | claude-opus-4-8 |
| `/speckit.implement` | 依任務實作 | **各受測模型** |

---

## 三個題目

| # | 題目 | 目標指標 | 輸入 | 輸出 |
|---|---|---|---|---|
| 001 | 多技術晶粒切割 Partitioning | 滿足面積/平衡限制下最小化切割 | `.txt`（Tech/LibCell/Die/Cell/Net） | `.out` |
| 002 | 固定輪廓平面規劃 Floorplanning | 固定輪廓內最小化線長 HPWL | `.txt`（Chip/Soft/Fixed module/Net） | `.floorplan` |
| 003 | 全域佈局 Global Placement | 最小化 HPWL | Bookshelf（`.aux/.nodes/.nets/.pl/.scl/.wts`） | `.gp.pl` |

各題詳細說明見該資料夾的 `README.md` 與 `reference/spec.pdf`。

---

## 環境需求

- **Claude Code** CLI（SDD planning + 各模型 implement）
- **計分器 `scorer/`**：純 Python 3.7+，**Windows 原生可跑**，無需編譯器或 Linux（見 [`scorer/README.md`](scorer/README.md)）
- **Portable C++ 編譯環境**：`tools\mingw64\`（WinLibs GCC 16.1.0 + MinGW-w64 14.0.0 UCRT），執行 `. .\tools\mingw64\setup-env.ps1` 即可使用，**無需另外安裝 g++**

> **編譯指令**：`g++ -std=c++20 -O3 -fopenmp -pthread -o <exe> main.cpp`

## 注意：大型測資不在 repo 內

`001-partitioning` 的大型測資（`public*.txt`，數十 MB）已由 `.gitignore` 排除，
僅保留 `sample.txt`。完整測資取得方式見 `problems/001-partitioning/benchmark/README.md`。
