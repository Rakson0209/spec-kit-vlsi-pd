# 001 — 多技術晶粒切割 (Die Partitioning)

> 清大 CS6135 VLSI Physical Design Automation, HW2。完整題目見 [`reference/spec.pdf`](reference/spec.pdf)。

## 題目

將電路中的所有 cell 分配到兩個晶粒 **DieA / DieB**，兩個 die 各使用不同的製程技術（Tech），
同一種 LibCell 在不同 Tech 下有不同的長寬。在滿足每個 die 的**面積使用率上限**與
**平衡限制**下，**最小化跨 die 的切割 (cut size)**。

## 輸入格式（`.txt`）

```
NumTechs <n>
Tech <TechName> <numLibCells>
LibCell <name> <width> <height>
...
DieSize <W> <H>
DieA <TechName> <utilization%>
DieB <TechName> <utilization%>
NumCells <n>
Cell <CellName> <LibCellName>
NumNets <n>
Net <NetName> <degree>
Cell <CellName>
...
```

小型範例見 [`benchmark/testcase/sample.txt`](benchmark/testcase/sample.txt)。

## 輸出格式（`.out`）

每個 cell 被指派到 DieA 或 DieB 的結果（詳見 spec.pdf）。

## 編譯與執行 baseline

> 需要 Boost C++ library（`-I /usr/local/include/boost/`）。

```sh
cd reference/src
mkdir -p ../bin
make                 # 產生 reference/bin/hw2
../bin/hw2 ../../benchmark/testcase/sample.txt out.out
```

## 驗證

```sh
# 純 Windows：用計分器 scorer（純 Python）檢查合法性並計算 cut size
python scorer/score.py 001 --output-dir <模型輸出目錄>
```

## 大型測資

`benchmark/testcase/public*.txt`（數十 MB）不在 repo 內，見 [`benchmark/README.md`](benchmark/README.md)。

## Baseline 指標門檻（要超越的目標）

最佳化指標為 **cut size（越小越好）**。下表 **Min 為要超越的門檻**（已知最佳結果），
`Reference` 為人類參考解（baseline）的實測值。runtime 上限約 **300s**（須在此之內完成）。

| testcase | **目標 Min（要 ≤ 此值，越低越好）** | Reference 參考解 | Reference runtime | Max（零分門檻） |
|---|---:|---:|---:|---:|
| public1 | **104**  | 193   | 0.03s   | 1,441   |
| public2 | **816**  | 3,666 | 1.59s   | 27,862  |
| public3 | **1,762**| 7,092 | 47.95s  | 103,659 |
| public4 | **982**  | 2,265 | 1.09s   | 12,421  |
| public5 | **297**  | 1,669 | 15.4s   | 48,964  |
| public6 | **5,159**| 10,281| 225.47s | 490,120 |

> 依憲章原則 VI（研究先行，超越基準）：各模型在 `research.md` 須以上表 **Min** 為要打敗的門檻，
> 並記錄自己各 testcase 的 cut size 與 runtime，對比 Min / Reference。

## 實驗

每個模型的完整 SDD 鏈與實作放 [`experiments/<model>/`](experiments/)；該模型自產的 SDD 產物放 `experiments/<model>/spec/`。所有模型共用同一份題目敘述（[`reference/spec.pdf`](reference/) + `benchmark/`）。
最佳化指標：**cut size**（越小越好），須通過 scorer 合法性檢查。
