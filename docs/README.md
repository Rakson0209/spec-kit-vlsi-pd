# docs — 實驗方法論與結果彙整

## 方法論

1. **固定題目，獨立規格**：所有模型從同一份題目敘述（各題 `reference/spec.pdf` + `benchmark/`）各自跑完整 SDD，產出**自己的** `spec.md`（置於 `experiments/<model>/spec/`）。共同基準是題目敘述，不是生成的 spec。
2. **隔離實作**：各模型的完整 SDD 鏈與實作都在 `experiments/<model>/`，不互相參考。
3. **統一評測**：對相同 `benchmark/testcase/` 執行，以 `scorer/`（純 Python）判定合法性與計分，記錄最佳化指標。
4. **三項數據**：通過/失敗、最佳化指標、時間或開發回合數。
5. **研究先行，超越基準**：實作前須在 `research.md` 以各題 README 的 **Min** 為門檻調研演算法，以超越 baseline 為目標（憲章原則 VI）。

## RQ

- **RQ1**：SDD 流程 vs. 直接實作（以 `reference/` 人類解為對照）對品質/效率的影響。
- **RQ2**：同一題目敘述下，不同模型各自產生 SDD（含 spec）與實作的正確率與最佳化品質差異。

## 結果彙整（待填）

### 001-partitioning（指標：cut size，越小越好）

| 模型 | 通過題數 | 平均 cut size | 備註 |
|------|---------|--------------|------|
| reference (human) | | | baseline |
| | | | |

### 002-floorplanning（指標：線長/面積，越小越好）

| 模型 | 通過題數 | 平均指標 | 備註 |
|------|---------|---------|------|
| reference (human) | | | baseline |
| | | | |

### 003-global-placement（指標：HPWL，越小越好）

| 模型 | 通過題數 | 平均 HPWL | 備註 |
|------|---------|----------|------|
| reference (human) | | | baseline |
| | | | |
