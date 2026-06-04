<!--
Sync Impact Report (v0.2.0 → v0.3.0)
=====================================
Version change: 0.2.0 → 0.3.0 (MINOR: 新增原則 VI 研究先行、超越基準)
Added principles:
  - VI. 研究先行，超越基準 — 實作前須在 research.md 記錄 baseline 門檻、調研演算法、選定有潛力
                              超越 baseline 的方案；以「超越」而非「接近」為第一優先
Templates updated:
  ✅ .specify/templates/plan-template.md — 最佳化優先級「更接近基準線」改為「達到並超越基準線」；
                                          研究階段要求先記錄 baseline 指標門檻
  ✅ CLAUDE.md — Core Principles 摘要新增第 6 項
Follow-up TODOs:
  - （已處理）舊佈局的 002 Qwen SDD 產物與 specs/ 範例已由使用者刪除；之後所有 SDD run
    一律置於 experiments/<model>/spec/

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

### II. 驗證器即真理 (Verifier as Ground Truth)
正確性由該題 `benchmark/verifier/verify` 判定，不以人工或模型自評為準。
未通過 verifier 的實作一律視為「未通過」，無論程式碼看起來多合理。

### III. 公平比較 (Fair Comparison)
比較模型時，唯一變因是「模型」；模型會影響**整條 SDD 流程**（spec → plan → tasks → implement）。
保持一致的共同基準是：**題目敘述**（該題 `reference/spec.pdf`）、測資 `benchmark/testcase/`、
驗證器 `benchmark/verifier/`、計分器 `scorer/`、硬體與編譯環境。
每個模型的完整 SDD 鏈（spec/plan/tasks）與實作獨立放在 `experiments/<model>/`，
不得互相參考或共用中間產物。

### IV. 可重現 (Reproducibility)
每次實驗須記錄：使用模型與版本、SDD 各階段產物、編譯指令、執行指令、
測資、verifier 結果、最佳化指標數值、時間/回合數。資訊不足以重現的實驗不計入結論。

### V. 量化最佳化品質 (Quantify Optimization Quality)
不只看「對不對」，更要看「好不好」。每題有明確最佳化目標
（cut size / 面積 / HPWL…），須以數值記錄並跨模型比較，作為品質高下依據。

### VI. 研究先行，超越基準 (Research-First, Beat the Baseline)
**寫任何實作 code 之前**，必須先完成研究：在 `research.md`（`/speckit.plan` 的 Phase 0 產物）中
1. **記錄 baseline 指標數值**作為要打敗的門檻（取自該題 `reference/` 的人類參考解）；
2. **調研並列出候選演算法／啟發式**及其取捨（複雜度、實作成本、預期品質）；
3. **選定一個有合理潛力超越 baseline 的方案**並說明理由。
設計與實作以「**達到並超越 baseline 的最佳化指標**」為第一優先（而非僅接近）。
未完成上述研究不得進入 `/speckit.tasks` 與 `/speckit.implement`。
若最終仍無法超越 baseline，須在 `RESULT.md` 記錄與 baseline 的差距及原因分析。

## 技術與評測標準

- baseline（人類參考解）位於各題 `reference/`，作為品質對照基準線。
- 共用資產：`benchmark/testcase/`（測資）與 `benchmark/verifier/`（驗證器），baseline 與所有模型實作共用。
- 編譯以 `g++ -std=c++11 -O3` 為基準；003 須連結題目提供的 `reference/obj/*.o` 預編譯函式庫。
- 評測三項數據缺一不可：通過/失敗、最佳化指標、時間或開發回合數。

## 實驗工作流程

1. 跑 `/speckit.specify` 時以 `SPECIFY_FEATURE_DIRECTORY=problems/<NNN-題目>/experiments/<model>/spec`
   指定該模型的 SDD 目錄；後續 `/speckit.plan` → `/speckit.tasks` → `/speckit.implement`
   會自動沿用同一目錄（選用 `/speckit.clarify`、`/speckit.analyze`）。
2. 每個模型的 SDD 產物入 `experiments/<model>/spec/`，實作入 `experiments/<model>/`。
   一次跑完一個模型的完整鏈再換下一個（`.specify/feature.json` 一次僅記一個 active 目錄）。
3. 對全部 `benchmark/testcase/` 執行並以 verifier / `scorer/` 驗證與計分。
4. 數據彙整入 `docs/`，更新比較表。

## Governance

本憲章凌駕個別實驗的便宜行事。任何偏離（例如為某模型放寬規格、跳過 verifier）
必須在該實驗紀錄中明確標註理由，否則該實驗結果不得納入跨模型比較結論。
修訂本憲章須在 commit 訊息說明變更與影響。

**Version**: 0.3.0 | **Ratified**: 2026-06-04 | **Last Amended**: 2026-06-04
