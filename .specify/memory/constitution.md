# spec-kit-vlsi-pd Constitution

本專案以 Spec-Driven Development (SDD) 流程重新實作 VLSI 實體設計題目，
並比較不同 LLM 模型的產出。以下原則約束所有實驗，確保結果客觀、可重現、可比較。

## Core Principles

### I. 規格先行 (Spec-First, NON-NEGOTIABLE)
任何實作必須先有 `spec.md`，再有 `plan.md` 與 `tasks.md`，最後才寫 code。
不允許跳過規格直接實作。規格須涵蓋輸入/輸出格式、限制條件、最佳化目標與驗收標準。
規格是模型間比較的「共同基準」——同一題目所有模型共用同一份規格，否則比較無效。

### II. 驗證器即真理 (Verifier as Ground Truth)
正確性由該題 `benchmark/verifier/verify` 判定，不以人工或模型自評為準。
未通過 verifier 的實作一律視為「未通過」，無論程式碼看起來多合理。

### III. 公平比較 (Fair Comparison)
比較模型時，唯一變因是「模型」。規格、測資、驗證器、評測腳本、硬體環境保持一致。
每個模型的實作獨立放在 `experiments/<model>/`，不得互相參考或共用中間產物。

### IV. 可重現 (Reproducibility)
每次實驗須記錄：使用模型與版本、SDD 各階段產物、編譯指令、執行指令、
測資、verifier 結果、最佳化指標數值、時間/回合數。資訊不足以重現的實驗不計入結論。

### V. 量化最佳化品質 (Quantify Optimization Quality)
不只看「對不對」，更要看「好不好」。每題有明確最佳化目標
（cut size / 面積 / HPWL…），須以數值記錄並跨模型比較，作為品質高下依據。

## 技術與評測標準

- baseline（人類參考解）位於各題 `reference/`，作為品質對照基準線。
- 共用資產：`benchmark/testcase/`（測資）與 `benchmark/verifier/`（驗證器），baseline 與所有模型實作共用。
- 編譯以 `g++ -std=c++11 -O3` 為基準；003 須連結題目提供的 `reference/obj/*.o` 預編譯函式庫。
- 評測三項數據缺一不可：通過/失敗、最佳化指標、時間或開發回合數。

## 實驗工作流程

1. `/speckit.specify` → `/speckit.plan` → `/speckit.tasks` → `/speckit.implement`（選用 `/speckit.clarify`、`/speckit.analyze`）。
2. SDD 產物入 `spec/`，實作入 `experiments/<model>/`。
3. 對全部 `benchmark/testcase/` 執行並以 verifier 驗證。
4. 數據彙整入 `docs/`，更新比較表。

## Governance

本憲章凌駕個別實驗的便宜行事。任何偏離（例如為某模型放寬規格、跳過 verifier）
必須在該實驗紀錄中明確標註理由，否則該實驗結果不得納入跨模型比較結論。
修訂本憲章須在 commit 訊息說明變更與影響。

**Version**: 0.1.0 | **Ratified**: 2026-06-04 | **Last Amended**: 2026-06-04
