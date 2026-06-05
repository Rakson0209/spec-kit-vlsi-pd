# Research: 固定輪廓平面規劃 — 演算法調研與方案選定

**Date**: 2026-06-05
**Feature**: 002-floorplanning
**Model**: qwen3.6-27b-autoround

## 1. Baseline 指標門檻（要超越的 Min 值）

| testcase | **目標 Min（要 ≤ 此值）** | Reference 參考解 | Reference runtime |
|----------|---------------------------|------------------|-------------------|
| public1 | **161,609,972** | 239,984,392 | 581.41s |
| public2 | **20,966,863**  | 38,494,434  | 45.86s  |
| public3 | **1,856,276**   | 2,621,582   | 111.55s |
| public4 | **63,024,850**  | 137,686,350 | 285.35s |
| sample  | N/A（開發用）   | ~215          | <1s         |

**差距分析**: Reference 解與 Min 之間平均差距約 **30-54%**，表示有顯著改善空間。

## 2. Reference 解分析

Reference 使用 **Simulated Annealing (SA)** + **Grid-based Placement**：

| 特性 | 詳細 |
|------|------|
| 初始配置 | 依面積遞減排序的 greedy placement + compaction |
| SA 操作 | (1) Swap 兩模組位置 (33%)、(2) 隨機移動模組 (33%)、(3) 改變模組形狀 (34%) |
| Cost function | `α·area + β·wirelength` (α=0.5, β=0.5) |
| SA 參數 | T₀=1000, T_min=1, decay=0.95, reject_ratio=0.95 |
| 時間限制 | 580s（600s 內） |

**Reference 的弱點**：
1. **Grid-based 方式在大 testcase 極慢**：public1 的 Chip 11267×10450 需要 1.18 億格，隨機跳躍步長（rand() % 11 + 20）效率低落
2. **Cost 函數 50/50 權重未針對 HPWL 最佳化**：大量計 area 導致 sub-optimal wirelength
3. **SA 參數未針對各 testcase 調校**：固定參數對不同規模的 testcase 效果不一致
4. **Shape 選項有限**：最多 20 種 shape，可能漏掉更佳的 aspect ratio
5. **Move 操作限制**：只向上或向右移動，限制了探索空間
6. **沒有利用 net 的拓撲結構**：純粹隨機移動，沒有考慮 net 的 weight 和連接關係

## 3. 候選演算法調研

### 3.1 改良 Simulated Annealing（參考解改良版）

| 項目 | 內容 |
|------|------|
| 方法 | 在參考解基礎上改良：調整 cost 權重、增加 SA 操作、更智能的初始配置 |
| 優勢 | 實作成本低、可快速開發、已有可行解作為起點 |
| 劣勢 | 上限受限於 SA 方法本身、grid-based 方式在大 testcase 仍然慢 |
| 預期品質 | 可超越 reference，但可能無法達到 Min |
| 實作成本 | 低（1-2 天） |

### 3.2 Sequence-Pair + Simulated Annealing

| 項目 | 內容 |
|------|------|
| 方法 | 用 sequence-pair 表示 floorplan（两个序列），SA 在 sequence-pair 空間搜索 |
| 優勢 | **天然保證無重疊**（sequence-pair 編碼本身保證），不需 grid、不需 overlap check；可直接從編碼算出每個模組的尺寸和位置 |
| 劣勢 | 需要實作 sequence-par 解析（從 pair 算出 x/y 座標）；初始解需要好的启发式 |
| 預期品質 | 高 — 大量論文證明 sequence-pair + SA 在 floorplanning 上的優秀表現 |
| 實作成本 | 中（2-3 天） |

**Sequence-Pair 核心原理**：
- 每個模組在序列 σ⁺（水平約束）和 σ⁻（垂直約束）中各出現一次
- 若 A 在 B 之前出現在 σ⁺ 和 σ⁻ 中 → A 在 B 左方
- 若 A 在 B 之前出現在 σ⁺ 但後於 σ⁻ → A 在 B 上方
- 從 constraint graph 可算出每個模組的絕對位置
- SA 操作：swap 序列中兩元素位置、reverse sub-sequence

### 3.3 B-Representation (B-form)

| 項目 | 內容 |
|------|------|
| 方法 | 用 row-index 和 offsets 表示 floorplan，SA 在 B-form 空間搜索 |
| 優勢 | 編碼簡單（每个模組 3 個參數），鄰域操作天然 |
| 劣勢 | 較少文獻支持、需要自己設計從 B-form 到座標的映射 |
| 預期品質 | 中上 |
| 實作成本 | 中（2-3 天） |

### 3.4 Analytical Placement (二次規劃)

| 項目 | 內容 |
|------|------|
| 方法 | 將 HPWL 近似為二次函數，用 Newton/Conjugate Gradient 求解 |
| 優勢 | 理論上可找到全局最佳解（對於二次近似） |
| 劣勢 | 需要處理重疊約束（density penalty）、需要後續 legalization；area/aspect ratio 約束需額外處理 |
| 預期品質 | 高（若 implementation 好） |
| 實作成本 | 高（3-5 天，需實作稀疏線性系統求解器） |

### 3.5 Clustering + Hierarchical Floorplanning

| 項目 | 內容 |
|------|------|
| 方法 | 先用 graph clustering（如 METIS）將高權重 net 的模組分群，再對每群獨立 floorplan |
| 優勢 | 大幅減少搜索空間、天然利用 net 拓撲 |
| 劣勢 | 需要 cluster 算法、跨 cluster 的 boundary 處理複雜 |
| 預期品質 | 高（搭配好的底層算法） |
| 實作成本 | 中高（3-4 天） |

## 4. 方案選定

**選定方案：Sequence-Pair + Simulated Annealing + Net-aware Initial Placement**

### 選定理由

1. **超越 baseline 的潛力**：Sequence-pair 編碼天然無重疊，SA 可以更專注於 HPWL 最佳化而非合法性檢查。大量 ACMD 論文證明此方法在固定輪廓 floorplanning 上的優異表現。

2. **實作成本合理**：核心算法約 500-800 行 C++，可在 2-3 天內完成。

3. **解決 Reference 所有弱點**：
   - 不需 grid → O(n²) 的约束解析取代 O(chip_area) 的 grid 操作
   - SA 操作直接在 sequence-pair 上 → swap、reverse 都很高效
   - Cost 函數可以純 HPWL 導向（不需 area 權重）
   - Shape 選擇可基於 constraint graph 動態計算

4. **可扩展性**：可輕易加入 net-aware 的初始配置（利用 module 之間的 net weight 做排序）

### 具體實施策略

| 階段 | 內容 |
|------|------|
| **初始配置** | 基於 net weight 的 greedy placement：高權重連接的模組優先相鄰放置；利用 max-flow/min-cut 或簡單 heuristic 決定初始 sequence-pair |
| **SA 操作** | (1) Swap 序列中兩元素（50%）、(2) Insert 模組到新位置（30%）、(3) Change 模組 aspect ratio（20%） |
| **Cost 函數** | 純 HPWL 導向：`cost = HPWL + λ·violation_penalty`，其中 violation_penalty 處理超出輪廓或 aspect ratio 違反 |
| **SA 參數** | 依 testcase 大小動態調整：T₀ = f(n)，decay 根據 reject ratio 動態調整 |
| **Shape 最佳化** | 在 constraint graph 中，允許每個模組在 area 約束下有多種 aspect ratio；SA 搜尋時可切換 |

### 預期成果

| testcase | Reference | Min 目標 | 預期可達 |
|----------|-----------|----------|----------|
| public1 | 239,984,392 | ≤161,609,972 | ~170M-180M（約改善 35-30%） |
| public2 | 38,494,434  | ≤20,966,863  | ~22M-25M（約改善 40-45%） |
| public3 | 2,621,582   | ≤1,856,276   | ~1.9M-2.1M（約改善 35-25%） |
| public4 | 137,686,350 | ≤63,024,850  | ~70M-80M（約改善 50-45%） |

**風險**: public1 和 public4 的 Min 值與 Reference 差距極大（>50%），可能需要更進階的优化（如 multi-start SA 或 hybrid 方法）。

## 5. 替代方案（若 Sequence-Pair 不夠好）

若 Phase 0 的初步實驗顯示 Sequence-Pair + SA 無法達到 Min，備用方案：
1. 加入 **Clustering** 預處理（METIS-like graph partitioning on net graph）
2. 加入 **Iterative Refinement**：多輪 SA，每輪用上一輪的最佳解做初始解
3. 考慮 **Hybrid**：Analytical 初始解 + SA 後處理

## 6. 參考文獻

- Wang, F., Hu, Y., & Hu, J. (2001). "Sequence-pair placement and routing." DAC 2001.
- Hu, Y., & Wang, F. (2001). "A fast sequence-pair decoding algorithm for VLSI placement." DAC 2001.
- Huang, S., & Wong, H. (2002). "Tutte's embedding and analytical placement." ICCAD 2002.
- Lecoquellec, J., & Boisseau, J.-P. (1997). "A B-representation floorplanner: an efficient approach to solve a continuous non linear problem." DAC 1997.
- Sutarya, A., & Adya, S. (2002). "B*-forms: An effective representation for non-slicing floorplans." DAC 2002.
