<!--
Sync Impact Report (v0.3.0 → v0.4.0)
=====================================
Version change: 0.3.0 → 0.4.0 (MINOR: 新增原則 VII 合法性門檻與完成定義；評測判定對齊 scorer)
Added principles:
  - VII. 合法性門檻與完成定義 (NON-NEGOTIABLE) — 全部 testcase 必須 100% 合法才算「完成」；
         合法性是 gate、最佳化是 objective，先全合法再談 ≤ Min；implement 須含 scorer
         驗證迴圈，對不合法者 debug 修正並重跑直到全合法，否則不得宣稱完成或結束
Modified:
  - 原則 II「驗證器即真理」→「評測即真理」：合法性/正確性改由 scorer/ 判定
    （純 Windows 無法執行 Linux 的 benchmark/verifier/verify）
  - 原則 III/IV、技術與評測標準、實驗工作流程、Governance：verifier 提及對齊 scorer/
Templates / docs（前次 commit 143c562 已先行）:
  ✅ 各題 README 驗收段、experiments RESULT 模板 → 改用 scorer（verifier 欄→合法(scorer)）

--- Prior change (v0.2.0 → v0.3.0) ---
新增原則 VI 研究先行、超越基準；實作前須在 research.md 記錄 baseline 門檻、調研演算法、選定方案。
--- Prior change (v0.1.1 → v0.2.0) ---
比較基準由「共用 spec」改為「每模型獨立 spec」：原則 I 移除共用規格、原則 III 重新定義共同基準為
「題目敘述 + benchmark + scorer + 環境」；SDD 產物改入 experiments/<model>/spec/。
=====================================
-->

# spec-kit-vlsi-pd Constitution

本專案以 Spec-Driven Development (SDD) 流程重新實作 VLSI 實體設計題目，
並比較不同 LLM 模型的產出。以下原則約束所有實驗，確保結果客觀、可重現、可比較。

## Core Principles

### I. 規格先行 (Spec-First, NON-NEGOTIABLE)
任何實作必須先有 `spec.md`，再有 `plan.md` 與 `tasks.md`，最後才寫 code。
不允許跳過規格直接實作。規格須涵蓋輸入/輸出格式、限制條件、最佳化目標與驗收標準。
**每個模型獨立產生自己的 spec / plan / tasks**：所有模型從**同一份題目敘述**
（該題 `reference/spec.pdf` 與 `benchmark/`）各自跑完整 SDD 流程，
SDD 產物放在該模型自己的 `experiments/<model>/spec/`。模型間不得共用或互相參考 SDD 產物。
共同基準是「題目敘述」而非「生成的規格」（見原則 III）。

### II. 評測即真理 (Scorer as Ground Truth)
正確性與**合法性**由專案根的 `scorer/`（純 Python，Windows 原生可跑）判定，
不以人工或模型自評為準。`scorer/` 對每個 testcase 檢查合法性（重疊、輪廓/core 邊界、
面積/使用率等限制）並重算最佳化指標；未通過合法性檢查的實作一律視為「未通過」，
無論程式碼看起來多合理。（官方 `benchmark/verifier/verify` 為 Linux ELF，純 Windows 不可用。）

### III. 公平比較 (Fair Comparison)
比較模型時，唯一變因是「模型」；模型會影響**整條 SDD 流程**（spec → plan → tasks → implement）。
保持一致的共同基準是：**題目敘述**（該題 `reference/spec.pdf`）、測資 `benchmark/testcase/`、
計分器 `scorer/`、硬體與編譯環境。
每個模型的完整 SDD 鏈（spec/plan/tasks）與實作獨立放在 `experiments/<model>/`，
不得互相參考或共用中間產物。

### IV. 可重現 (Reproducibility)
每次實驗須記錄：使用模型與版本、SDD 各階段產物、編譯指令、執行指令、
測資、`scorer/` 合法性與計分結果、最佳化指標數值、時間/回合數。資訊不足以重現的實驗不計入結論。

### V. 量化最佳化品質 (Quantify Optimization Quality)
不只看「對不對」，更要看「好不好」。每題有明確最佳化目標
（cut size / 面積 / HPWL…），須以數值記錄並跨模型比較，作為品質高下依據。

### VI. 研究先行，超越基準 (Research-First, Beat the Baseline)
**寫任何實作 code 之前**，必須先完成研究：在 `research.md`（`/speckit.plan` 的 Phase 0 產物）中
1. **記錄 baseline 指標數值**作為要打敗的門檻（取自該題 `reference/` 的人類參考解與各題 README 的 Min）；
2. **調研並列出候選演算法／啟發式**及其取捨（複雜度、實作成本、預期品質）；
3. **選定一個有合理潛力超越 baseline 的方案**並說明理由。
設計與實作以「**達到並超越 baseline 的最佳化指標**」為第一優先（而非僅接近）。
未完成上述研究不得進入 `/speckit.tasks` 與 `/speckit.implement`。
若最終仍無法超越 baseline，須在 `RESULT.md` 記錄與 baseline 的差距及原因分析。

### VII. 合法性門檻與完成定義 (Legality Gate & Definition of Done, NON-NEGOTIABLE)
合法性是**硬性門檻 (gate)**，優先於一切最佳化目標 (objective)。先後順序不可顛倒：

1. **完成定義 (DoD)**：一個實作要算「完成」，必須對**全部 testcase** 跑 `scorer/`，
   並**100% 合法**（無重疊、在輪廓/core 範圍內、面積/使用率等限制全部滿足）。
   只要有**任一** testcase 不合法，該實作即**未完成**——不是「分數低」，是「根本沒做完」，
   **不得宣稱完成或結束**。
2. **優先級**：先做到「全部 testcase 合法」（gate），**再**追求最佳化指標 ≤ Min（objective）。
   不得為了壓低指標而犧牲合法性；**不合法的低指標一律視為失敗（0 分）**，不納入比較。
3. **implement 必含驗證迴圈**：`/speckit.implement` 必須實際以 `scorer/` 逐 testcase 驗證合法性，
   對不合法者**定位原因、修正、重跑**，反覆直到全部合法為止，才能結束。
   不允許「能產生輸出檔」就視為 implement 完成。
4. **紀錄**：`RESULT.md` 必須逐 testcase 記錄合法（OK/NG）與指標數值；
   只要存在任一 NG，即代表該模型此題**尚未完成**，須在 RESULT.md 標明並繼續修正。

## 技術與評測標準

- baseline（人類參考解）位於各題 `reference/`，作為品質對照基準線。
- 共用資產：`benchmark/testcase/`（測資）；合法性與計分一律用專案根的 `scorer/`（純 Python，Windows 可跑）。
- 編譯以 `g++ -std=c++11 -O3` 為基準（純 Windows 用 WinLibs g++；自編 exe 須在允許執行的目錄下跑）。
- 評測三項數據缺一不可：合法/不合法（全 case）、最佳化指標、時間或開發回合數。

## 實驗工作流程

1. 跑 `/speckit.specify` 時以 `SPECIFY_FEATURE_DIRECTORY=problems/<NNN-題目>/experiments/<model>/spec`
   指定該模型的 SDD 目錄；後續 `/speckit.plan` → `/speckit.tasks` → `/speckit.implement`
   會自動沿用同一目錄（選用 `/speckit.clarify`、`/speckit.analyze`）。
2. 每個模型的 SDD 產物入 `experiments/<model>/spec/`，實作入 `experiments/<model>/`。
   一次跑完一個模型的完整鏈再換下一個（`.specify/feature.json` 一次僅記一個 active 目錄）。
3. `/speckit.implement` 須對全部 `benchmark/testcase/` 以 `scorer/` 驗證合法性與計分，
   **全部合法**方可結束（原則 VII）；再評估是否 ≤ Min（原則 VI）。
4. 數據彙整入 `docs/`，更新比較表。

## Governance

本憲章凌駕個別實驗的便宜行事。任何偏離（例如為某模型放寬規格、跳過 `scorer/` 合法性檢查、
或在仍有 testcase 不合法時宣稱完成）必須在該實驗紀錄中明確標註理由，
否則該實驗結果不得納入跨模型比較結論。修訂本憲章須在 commit 訊息說明變更與影響。

**Version**: 0.4.0 | **Ratified**: 2026-06-04 | **Last Amended**: 2026-06-05
