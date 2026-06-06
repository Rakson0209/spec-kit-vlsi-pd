# spec-kit-vlsi-pd

以 **Spec-Driven Development (SDD)** 流程，搭配 [GitHub spec-kit](https://github.com/github/spec-kit) 與 [Qwen Code](https://github.com/QwenLM/qwen-code)，
重新實作 **VLSI 實體設計自動化 (Physical Design Automation)** 的經典演算法題目，
並用以**測試 SDD 開發流程套用在 VLSI PD 上的效果**，以及**比較不同 LLM 模型的產出品質**。

---

## 為什麼做這件事（第一性原理）

VLSI 實體設計題目具備三個利於做 SDD / 模型比較的本質特性：

1. **規格明確**：輸入/輸出格式固定、評分指標客觀（cut size、面積、HPWL…），可被機器驗證。
2. **可機器評測**：每題有客觀的合法性規則與指標，計分器可自動判定對錯，不需人工判讀。
3. **有量化品質指標**：不只「對不對」，還有「好不好」（最佳化目標），能拉出模型間的高下。

因此這是一個能**客觀量化** 「SDD 流程價值」與「模型能力差異」的理想實驗場。

---

## 實驗目標

- **RQ1**：把人類拍腦袋直接寫 code 換成 SDD 流程（spec → plan → tasks → implement），對 VLSI PD 題目的產出品質與開發效率有何影響？
- **RQ2**：在相同 SDD 規格下，不同模型（如 Qwen-Coder 各尺寸、其他模型）的實作正確率與最佳化品質差異多大？

每個實驗都以三項客觀數據對比：**是否合法（scorer 檢查）**、**最佳化指標數值**、**執行時間 / 開發回合數**。

---

## 資料夾架構

```
spec-kit-vlsi-pd/
├── .specify/                 # spec-kit 引擎（templates / scripts(ps) / workflows / constitution）
│   └── memory/constitution.md
├── .qwen/commands/           # Qwen Code 的 SDD slash 命令（/speckit.*）
├── QWEN.md                   # Qwen Code 的 agent context
├── problems/                 # ★ 依題目分組
│   ├── 001-partitioning/         # 多技術晶粒切割 (Die Partitioning)
│   ├── 002-floorplanning/        # 固定輪廓平面規劃 (Fixed-outline Floorplanning)
│   └── 003-global-placement/     # 全域佈局 (Global Placement, 最小化 HPWL)
│       ├── reference/            # 人類參考解（baseline）：原始碼、spec.pdf、report.pdf
│       ├── benchmark/            # 共用測資 testcase/（評測用根目錄的 scorer/，純 Python）
│       └── experiments/          # 各模型的完整 SDD 鏈 + 實作 + 評測結果
│           └── <model>/
│               ├── spec/         # 該模型自產的 SDD 產物（spec.md / plan.md / tasks.md）
│               └── ...           # 該模型的實作與評測數據
├── scorer/                   # ★ 批次計分器（重算指標 + 合法性 + 彙整表格）
├── docs/                     # 實驗方法論、結果彙整
└── README.md
```

每個 `problems/NNN-*/` 內固定三個子目錄：

| 子目錄 | 用途 |
|---|---|
| `reference/` | 人類參考解（清大 CS6135 作業原始碼）+ 原始題目 `spec.pdf` + `report.pdf`，作為 baseline；**`spec.pdf` 是餵給所有模型的共同題目敘述** |
| `benchmark/` | `testcase/`（共用測資），baseline 與各模型實作共用同一份；評測與計分一律用專案根的 `scorer/`（純 Python） |
| `experiments/` | 各模型 `<model>/` 各自一份，內含**該模型自產的 SDD 產物 `spec/`**（`spec.md`/`plan.md`/`tasks.md`）、實作與評測數據 |

> **比較基準（v0.2.0 起）**：每個模型從**同一份題目敘述**（`reference/spec.pdf` + `benchmark/`）
> 各自跑完整 SDD 流程，產出**獨立的 spec**。共同基準是「題目敘述 + 測資 + 驗證器 + 計分器 + 環境」，
> 唯一變因是「模型」（模型影響整條 SDD 鏈）。詳見 `.specify/memory/constitution.md` 原則 I、III。

---

## SDD 工作流程（spec-kit）

> **照著操作的完整手冊見 [`PROMPT.md`](PROMPT.md)** —— 含 001/002/003 三題、Claude Code 與 Qwen Code
> 兩 agent 的逐步命令、編譯/執行/計分指令、Min 門檻速查。本節僅列命令總覽。

在專案根目錄用 Claude Code 或 Qwen Code 開啟，依序使用 slash 命令：

| 命令 | 作用 |
|---|---|
| `/speckit.constitution` | 建立/更新專案開發原則（已有初版於 `.specify/memory/constitution.md`） |
| `/speckit.specify` | 由題目需求產生規格 `spec.md` |
| `/speckit.clarify` | （選用）對模糊處提問澄清，降低風險 |
| `/speckit.plan` | 產生技術實作計畫 `plan.md` |
| `/speckit.tasks` | 拆解為可執行任務 `tasks.md` |
| `/speckit.analyze` | （選用）跨文件一致性檢查 |
| `/speckit.implement` | 依任務實作 |

> spec-kit 已用 `--ai qwen --script ps`（PowerShell 腳本）初始化。

---

## 如何進行一次模型比較實驗

1. 選定題目，例如 `problems/001-partitioning/`。
2. 以某模型跑完整 SDD 流程；跑 `/speckit.specify` 時以
   `SPECIFY_FEATURE_DIRECTORY=problems/<NNN-題目>/experiments/<model>/spec` 指定該模型的 SDD 目錄，
   後續 `/speckit.plan`、`/speckit.tasks`、`/speckit.implement` 會自動沿用，產物全落在 `experiments/<model>/`。
3. 用 `scorer/score.py` 對 `benchmark/testcase/` 批次計分：自動**重算最佳化指標**、檢查合法性，
   並輸出 Markdown/CSV（純 Python，Windows 原生可跑；見 [`scorer/README.md`](scorer/README.md)）。
4. scorer 報表彙整至 `docs/`；記錄三項數據：**通過/失敗**、**最佳化指標**、**時間/回合數**。
5. 換下一個模型，重複：**題目敘述（`reference/spec.pdf`）與評測資產保持一致**，由模型各自重新產生 spec，只變動模型。

> 評測一律用 `scorer`（純 Python、Windows 原生可跑）：檢查合法性、重算指標數值、跨模型彙整，是做比較的核心工具。

---

## 三個題目

| # | 題目 | 目標指標 | 輸入 | 輸出 |
|---|---|---|---|---|
| 001 | 多技術晶粒切割 Partitioning | 滿足面積/平衡限制下最小化切割 | `.txt`（Tech/LibCell/Die/Cell/Net） | `.out` |
| 002 | 固定輪廓平面規劃 Floorplanning | 固定輪廓內最小化線長/面積 | `.txt`（Chip/Soft/Fixed module/Net） | `.floorplan` |
| 003 | 全域佈局 Global Placement | 最小化 HPWL | Bookshelf（`.aux/.nodes/.nets/.pl/.scl/.wts`） | `.gp.pl` |

各題詳細說明見該資料夾的 `README.md` 與 `reference/spec.pdf`。

---

## 環境需求

- **Qwen Code** CLI（SDD agent）
- **spec-kit**（`specify` CLI，已初始化）
- **計分器 `scorer/`**：純 Python 3.7+，**Windows 原生可跑**，無需編譯器或 Linux（見 [`scorer/README.md`](scorer/README.md)）
- **Portable C++ 編譯環境**：專案已內建在 `tools\mingw64\`（WinLibs GCC 16.1.0 + MinGW-w64 14.0.0 UCRT），
  解壓縮後執行 `tools\mingw64\setup-env.bat` 或 `. .\tools\mingw64\setup-env.ps1` 即可使用，
  **無需另外安裝 g++**。其他電腦解壓縮後同樣適用。

> **平台註記**：`baseline`（001 需 Boost、003 為 Linux 預編譯 `obj`）多屬 Linux 工具，純 Windows 未必能執行/連結。
> 因此在純 Windows 下，合法性與指標計算一律用 `scorer/`（純 Python，已涵蓋各題主要規則）。

## 注意：大型測資不在 repo 內

`001-partitioning` 的大型測資（`public*.txt`，數十 MB）已由 `.gitignore` 排除，
僅保留 `sample.txt`。完整測資取得方式見 `problems/001-partitioning/benchmark/README.md`。
