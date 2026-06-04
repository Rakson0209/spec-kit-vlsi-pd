# I/O Format Contract

**Date**: 2026-06-04

## 輸入格式 (`.txt`)

```
ChipSize <W> <H>

NumSoftModules <n>
SoftModule <name> <area>
...

NumFixedModules <n>
FixedModule <name> <x> <y> <w> <h>
...

NumNets <n>
Net <moduleA> <moduleB> <weight>
...
```

### 欄位說明

| 欄位 | 類型 | 說明 |
|---|---|---|
| `W`, `H` | int | 晶片輪廓寬度與高度 |
| `name` | string | 模組名稱（不含空白） |
| `area` | int | 軟模組最小面積 |
| `x`, `y` | int | 硬模組左下角座標 |
| `w`, `h` | int | 硬模組寬度與高度 |
| `moduleA`, `moduleB` | string | net 連接的兩個模組名稱（可為軟/硬） |
| `weight` | int | net 權重 |

### 解析規則
- 每行以空白分隔的 tokens
- 空行作為段落分隔（parser 需跳過空行）
- 模組名稱大小寫敏感

## 輸出格式 (`.floorplan`)

```
Wirelength <value>

NumSoftModules <n>
<name> <x> <y> <w> <h>
...
```

### 欄位說明

| 欄位 | 類型 | 說明 |
|---|---|---|
| `value` | int/float | 計算出的加權 HPWL 值 |
| `n` | int | 軟模組數量（與輸入相同） |
| `name` | string | 軟模組名稱 |
| `x`, `y` | int | 左下角座標，`≥ 0` |
| `w`, `h` | int | 寬度與高度，`> 0` |

### 輸出規則
- 首行 `Wirelength` 為可選（但建議輸出以方便比對）
- 硬模組**不**出現在輸出中（其位置由輸入決定）
- 軟模組輸出順序不限

## HPWL 計算

```
cx = x + floor(w / 2)
cy = y + floor(h / 2)
HPWL = Σ weight_i × (|cx_a - cx_b| + |cy_a - cy_b|)
```

- `floor(w/2)` 即整數除法 `w / 2`（C++ `int` 除法自動向下取整）
- 模組中心的 `cx`, `cy` 為整數
- net 中的模組可為軟模組或硬模組
