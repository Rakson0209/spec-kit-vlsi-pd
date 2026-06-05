# claude-opus-4-8 — 002-floorplanning 結果

- 模型 / 版本：Claude Opus 4.8（`claude-opus-4-8`，1M context）
- SDD 產物（本模型自產）：見 [`./spec/`](./spec/)（spec / plan / research / data-model / contracts / tasks）
- 演算法：線長導向建構式擺放 + **無網格 HPWL-only 模擬退火**（O(N²) 重疊檢查、增量 ΔHPWL、reheat-intensify 多回合）
- 編譯指令：`g++ -std=c++11 -O3 -o hw3 main.cpp`
- 執行指令：`hw3 <input.txt> <output.floorplan>`（可選 `argv[3]`＝時間上限秒數；預設 120）
- 隨機 seed：`20260605`（固定，mt19937）→ 可重現
- 時間預算：預設 120s/測資（≤600s 硬上限內，兼顧 public2/3/4 的 ≤120s 目標）

## 評測結果（時間預算 120s，seed 20260605）

| testcase | 合法性 | wirelength (HPWL) | 執行時間 | 目標 Min | 對 Min | Reference | 對 Reference |
|----------|--------|------------------:|---------:|---------:|:------:|----------:|:-----------:|
| sample   | 合法   | 215               | ~0s*     | —        | —      | —         | —           |
| public1  | 合法   | **161,052,646**   | 120s     | 161,609,972 | **✅ −0.34%（超越）** | 239,984,392 | −32.9% |
| public2  | 合法   | 21,893,463        | 120s     | 20,966,863  | +4.42% | 38,494,434  | −43.1% |
| public3  | 合法   | 1,984,464         | 120s     | 1,856,276   | +6.91% | 2,621,582   | −24.3% |
| public4  | 合法   | 63,526,450        | 120s     | 63,024,850  | +0.80% | 137,686,350 | −53.9% |

\* sample 解立即收斂（Wirelength 215，與 baseline 一致）；外層退火會用滿時間預算但結果不變。

- 合法性：5/5 全部合法（見下方「驗證方式」說明）。
- 開發回合數：約 6 次「編譯—執行—計分」調參迭代（初版 → reheat 外迴圈 → 增量 ΔHPWL 加速 → 排程/算子調校 → 多起點/連線導向實驗（經實測回退）→ 定版）。
- 人工介入次數：0（全程由 agent 依 SDD 鏈自動完成）。

## 驗證方式（重要）

本機為純 Windows 環境，官方 `benchmark/verifier/verify` 為 **Linux ELF 無法執行**。
合法性改以**跨平台計分器** [`scorer/`](../../../../scorer/)（`scorer/lib/floorplanning.py`）判定——
其合法性檢查與 HPWL 計算**逐項複刻官方 verifier**：
- 軟模組落在輪廓內 `x+w≤W, y+h≤H`、面積 `w·h ≥ min_area`、長寬比 `h/w ∈ [0.5, 2]`；
- 所有模組（軟+硬）兩兩不重疊；硬模組位置與輸入一致；
- 加權 HPWL = Σ weight·(|cx₁−cx₂|+|cy₁−cy₂|)，中心 `(x+w/2, y+h/2)` 向下取整。

計分器對 5 份測資皆回報 `valid=True`。**若在 Linux 上跑官方 `verify` 預期同樣 `[Success]`**
（輸出 `Wirelength` 以同一整數中心公式計算，與計分器一致）。

計分指令：
```sh
python scorer/score.py 002 \
  --output-dir problems/002-floorplanning/experiments/claude-opus-4-8 \
  --label claude-opus-4-8
```

## 與 baseline / Min 的差距分析（憲章原則 VI）

**結論**：對人類參考解（baseline / Reference）**4/4 全面大幅超越**（−24% ~ −54%）；
對「已知最佳」門檻 **Min**，public1 **超越**、public4 僅差 0.80%、public2/public3 分別差 4.4% / 6.9%。

- **為何能大幅勝過 baseline**：baseline（`reference/`）以全晶片布林網格 + 含面積項的 cost 做 SA，
  單步成本 O(晶片面積)（public1 達 1.18 億格），迭代量被嚴重壓低。本實作改用
  **絕對座標 + O(N²) 重疊檢查 + 增量 ΔHPWL**（N≤28），單步近免費、cost 純線長，
  同樣時間內有效迭代量提升數個量級，故全面超越 Reference。
- **public2（+4.4%）**：晶片利用率 93.2%（極高密度）。可用空白極少，多數單模組搬移／交換找不到合法位置，
  SA 探索空間受限，難再壓低。突破需要「漣漪式多模組協同重排」或更強的合法化推擠，屬後續工作。
- **public3（+6.9%）**：模組最多（28 soft、108 nets）。局部搜尋在大規模、高連線密度下較易陷入局部最優；
  曾試「連線導向建構 + 多起點」但實測反而搶走 intensify 時間而退步，故回退單純 reheat-intensify。
  更佳解可能需解析式全域擺放（二次線長）+ 穩健合法化。
- **public4（+0.80%）**：已非常接近 Min；給更長時間（~300s，仍 ≤600s）有機會跨越，
  但為遵守 public2/3/4 ≤120s 目標，定版採 120s。

> 已達成 SC-001（合法率 100%）、SC-003（全測資優於 Reference 且遠低於 Max）、SC-004（≤120s≤600s）、
> SC-005（固定 seed 可重現，重跑差異 <1%）。SC-002（4 份全超越 Min）達成 public1，其餘記錄差距如上。

## 備註

- 形狀候選在生成階段即保證 `w·h ≥ area` 且 `h/w ∈ [0.5, 2]`，從源頭排除面積/長寬比違規。
- 輸出 `Wirelength` 以與計分器/verifier 相同的整數中心公式計算，避免 `Wrong Wirelength`。
- 更長時間預算（`hw3 in out 580`）對各測資邊際效益遞減（public1/public2 已早期 plateau）。
