# 002 — 固定輪廓平面規劃 (Fixed-outline Floorplanning)

> 清大 CS6135 VLSI Physical Design Automation, HW3。完整題目見 [`reference/spec.pdf`](reference/spec.pdf)。

## 題目

在固定的晶片輪廓 `ChipSize W H` 內，擺放 **soft modules**（面積給定、長寬可變形）與
**fixed modules**（位置與尺寸固定），使所有模組不重疊且落在輪廓內，
並依 net 連接關係**最小化線長**。

## 輸入格式（`.txt`）

```
ChipSize <W> <H>
NumSoftModules <n>
SoftModule <name> <area>
NumFixedModules <n>
FixedModule <name> <x> <y> <w> <h>
NumNets <n>
Net <moduleA> <moduleB> <weight>
```

小型範例見 [`benchmark/testcase/sample.txt`](benchmark/testcase/sample.txt)。

## 輸出格式（`.floorplan`）

各模組最終座標與尺寸（詳見 spec.pdf）。

## 編譯與執行 baseline

```sh
cd reference/src
mkdir -p ../bin
make                 # 產生 reference/bin/hw3
../bin/hw3 ../../benchmark/testcase/sample.txt out.floorplan
```

## 驗證

```sh
benchmark/verifier/verify <input.txt> <output.floorplan>
# 通過會印 [Success]，否則印 [Error] ...
```

## 實驗

各模型實作放 [`experiments/<model>/`](experiments/)，SDD 產物放 [`spec/`](spec/)。
最佳化指標：**線長 / 面積**（越小越好），須在固定輪廓內且通過 verifier。
