# spec-kit-vlsi-pd Constitution (v0.5.0)

## 硬規則（違反即失敗）

### R1. Baseline Fallback — 最重要
**先自己寫 code。跑 scorer。若任何 testcase 的指標比 `reference/src/` 差 → 立刻放棄自己的 code，複製 `reference/src/*.cpp` 到 `experiments/<model>/`，在此基礎上優化。**

不允許在自寫 code 上花超過一輪迭代。baseline 是全 case 的及格線。

### R2. Boost + 平行計算優先
寫 code 時**第一優先使用**：
- **Boost**：`boost::graph`、`boost::geometry`、`boost::multi_array` → `-I tools/boost`
- **OpenMP**：`-fopenmp`（平行 SA、平行 cost eval）
- **pthread**：`-pthread`（多執行緒多起點搜尋）

不允許只用單執行緒 STL。編譯一律 `-std=c++20 -O3 -fopenmp -pthread`。

### R3. 合法性硬門檻
所有 testcase 必須 scorer 合法（OK）。任一 NG = 未完成，不得宣稱結束。

### R4. 全 case 守門
每次改 code 後對**所有** testcase 跑 scorer。禁止只盯一個 case。

### R5. 規格先行
spec → plan → tasks → code，不可跳步。

### R6. 評測即真理
scorer 結果為唯一判斷標準。

## 技術標準

- 編譯：`g++ -std=c++20 -O3 -fopenmp -pthread`
- 環境：`tools\mingw64\setup-env.bat`（內建 portable g++ 16.1.0）
- 計分：`python scorer/score.py`

## 實驗流程

1. `/speckit.specify` → 產生 spec
2. `/speckit.plan` → 研究 + 計畫（research.md 必須記錄 baseline 門檻）
3. `/speckit.tasks` → 任務拆解
4. `/speckit.implement` → **R1 先行**：自寫 code → scorer 驗證 → 比 baseline 差？→ 複製 reference/src/ → 在此基礎上優化

**Version**: 0.5.0 | **Ratified**: 2026-06-04 | **Last Amended**: 2026-06-06
