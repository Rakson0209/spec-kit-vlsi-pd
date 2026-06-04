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
benchmark/verifier/verify <input.txt> <output.out>
# 通過會印 [Success]，否則印 [Error] ...
```

## 大型測資

`benchmark/testcase/public*.txt`（數十 MB）不在 repo 內，見 [`benchmark/README.md`](benchmark/README.md)。

## 實驗

各模型實作放 [`experiments/<model>/`](experiments/)，SDD 產物放 [`spec/`](spec/)。
最佳化指標：**cut size**（越小越好），須通過 verifier。
