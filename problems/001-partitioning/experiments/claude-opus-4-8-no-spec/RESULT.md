# 001 — Die Partitioning · claude-opus-4-8 實作結果

## 摘要

**7/7 testcase 全部合法（scorer 實測），6 個 public 全部 ≤ Min。** 自寫程式（R1 = 保留自寫，
不需 fallback 到 reference）。

| testcase | **Min（門檻）** | **本實作 cut** | 對 Min | Reference | 對 Ref | Max（零分） | 合法 |
|---|---:|---:|---:|---:|---:|---:|:--:|
| public1 | 104  | **80**   | −23% | 193    | 0.41× | 1,441   | ✅ |
| public2 | 816  | **798**  | −2.2%| 3,666  | 0.22× | 27,862  | ✅ |
| public3 | 1,762| **1,121**| −36% | 7,092  | 0.16× | 103,659 | ✅ |
| public4 | 982  | **863**  | −12% | 2,265  | 0.38× | 12,421  | ✅ |
| public5 | 297  | **230**  | −23% | 1,669  | 0.14× | 48,964  | ✅ |
| public6 | 5,159| **4,053**| −21% | 10,281 | 0.39× | 490,120 | ✅ |
| sample  | —    | 1        | —    | —      | —     | —       | ✅ |

> 數值為搜尋預算 `PART_TIME=120s` 的提交輸出（`out/*.out`）。**正式 exe 預設預算 280s**，
> 同種演算法在更長預算下只會持平或更佳。合法性以 `scorer/score.py 001` 全數 `valid=True`。

## 方法（第一性原理）

問題本質 = **約束型超圖二分最小切割**，兩 die 面積上限**不對稱**（同一 LibCell 在
TechA / TechB 面積不同）：
`Σ_{A} areaA[c] ≤ utilA·dieArea`，`Σ_{B} areaB[c] ≤ utilB·dieArea`，最小化跨 die 的 net 數。

最強槓桿 = **多層次 FM（multilevel FM）+ 平行多起點**：

1. **L1 解析**：整檔讀入 + 指標式 tokenizer（處理 60MB 的 public6）；CSR 雙向（net↔cell）。
2. **L2 多層次**：
   - **Coarsening**：heavy-edge matching（權重 `Σ 1/(deg−1)`，跳過超大 net）配對同質點。
   - **最粗層初始分割**：best-fit decreasing（先放最大 cell，挑剩餘相對 slack 較大的 die），
     保證可行。
   - **Uncoarsen + FM 精修**：gain-bucket Fiduccia-Mattheyses，逐層投影回細粒度後精修。
   - **V-cycle**：沿當前切割「只併同側」再 coarsen→refine，反覆逃離局部最佳。
3. **L3 平行多起點（OpenMP）**：時間預算內持續發起獨立 multilevel run，保留最佳「合法」cut。

### 關鍵突破：**鬆弛平衡的 FM**（overturns 直覺）

public5 一度卡在 cut≈926（Min 297，差 3×）。診斷：public5 的 TechB cell 面積約為 TechA 的
2.3×，兩 die cap 皆 80%，**面積極緊**——兩側都被填到使用率上限（utilB=0.800），標準 FM 在
cap 邊緣**幾乎無法移動任何 cell**（單步移動會立即超限），於是被鎖死在差解。

修正：FM pass **允許暫時超出 cap（`cap·(1+relax)`）讓 cell 流動**，但**只把「嚴格合法」的
最佳前綴**當候選解快照、結束時回捲到該點。一次到位把 public5 從 926 →**230**。

進一步以**多樣化 relax**（各起點取 {0.02, 0.05, 0.10, 0.20} 之一）兼顧：緊 cap 案例需大
relax（public5），鬆 cap 案例小 relax 更穩（public1/2），取全池最佳 → 三者同時最優。

## 合法性保證

- FM 每趟只回捲到「嚴格合法（`area ≤ cap + 1e-9`，與 scorer 同 eps）」最佳前綴 → 任一層輸出皆合法。
- 多層次投影保面積和不變 → 細粒度仍合法。
- 主迴圈後另有 feasibility 驗證 + greedy 修復作為安全網（實測未觸發）。

## 編譯與執行

```bat
build.bat                         REM -> D:\FSecret\hw2.exe（安全政策唯一可執行目錄）
D:\FSecret\hw2.exe <in.txt> <out.out>
set PART_TIME=120 & D:\FSecret\hw2.exe ...   REM 可調搜尋預算（秒），預設 280
```

編譯旗標：`g++ -std=c++20 -O3 -fopenmp -pthread`（R2）。

## 計分

```powershell
python scorer/score.py 001 --output-dir problems/001-partitioning/experiments/claude-opus-4-8/out --label claude-opus-4-8
```

報表：[score.md](score.md) · [score.csv](score.csv) · 輸出：[out/](out/) · 程式：[main.cpp](main.cpp)
