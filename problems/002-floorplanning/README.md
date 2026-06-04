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

## Baseline 指標門檻（要超越的目標）

最佳化指標為 **wirelength（線長，越小越好）**。下表 **Min 為要超越的門檻**（已知最佳結果），
`Reference` 為人類參考解（baseline）的實測值。runtime 上限約 **600s**（須在此之內完成）。

| testcase | **目標 Min（要 ≤ 此值，越低越好）** | Reference 參考解 | Reference runtime | Max（零分門檻） |
|---|---:|---:|---:|---:|
| public1 | **161,609,972** | 239,984,392 | 581.41s | 349,768,634 |
| public2 | **20,966,863**  | 38,494,434  | 45.86s  | 41,569,628  |
| public3 | **1,856,276**   | 2,621,582   | 111.55s | 5,045,921   |
| public4 | **63,024,850**  | 137,686,350 | 285.35s | 201,625,050 |

> 依憲章原則 VI（研究先行，超越基準）：各模型在 `research.md` 須以上表 **Min** 為要打敗的門檻，
> 並記錄自己各 testcase 的 wirelength 與 runtime，對比 Min / Reference。

## 實驗

每個模型的完整 SDD 鏈與實作放 [`experiments/<model>/`](experiments/)；該模型自產的 SDD 產物放 `experiments/<model>/spec/`。所有模型共用同一份題目敘述（[`reference/spec.pdf`](reference/) + `benchmark/`）。
最佳化指標：**線長 / 面積**（越小越好），須在固定輪廓內且通過 verifier。
