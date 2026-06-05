# Output Format Contract: `.floorplan`

## Overview

輸出檔為純文字格式，記錄所有軟模組的最終配置。硬模組不需要輸出（位置已固定）。

## Structure

```
Wirelength <value>

NumSoftModules <n>
<name_1> <x_1> <y_1> <w_1> <h_1>
<name_2> <x_2> <y_2> <w_2> <h_2>
...
```

## Field Specifications

### Header (Line 1)
| Field | Type | Constraint | Notes |
|-------|------|------------|-------|
| `Wirelength <value>` | keyword + int | `≥ 0` | 可選首行；為程式自算的 HPWL 值 |

### Section Header
| Field | Type | Constraint |
|-------|------|------------|
| `NumSoftModules <n>` | keyword + int | `> 0`；等於輸入檔的 NumSoftModules |

### Module Line（每行一個模組）
| Position | Field | Type | Constraint |
|----------|-------|------|------------|
| 1 | `name` | `string` | 對應到輸入檔中某 SoftModule 的 name |
| 2 | `x` | `int` | `≥ 0`；`x + w ≤ ChipSize.W` |
| 3 | `y` | `int` | `≥ 0`；`y + h ≤ ChipSize.H` |
| 4 | `w` | `int` | `> 0`；`w × h ≥ 該模組的 min_area` |
| 5 | `h` | `int` | `> 0`；`0.5 ≤ h/w ≤ 2.0` |

**Module Order**: 建議依輸入檔中 SoftModule 的出現順序排列（scorer 依 name 索引，順序不影響計分，但方便對照）。

## Example

```
Wirelength 215

NumSoftModules 2
GPU 3 0 5 5
CPU 0 5 4 3
```

## Validation Rules

輸出檔必須滿足以下條件才算合法（由 `scorer/score.py` 和 `benchmark/verifier/verify` 檢查）：

1. **完整性**: 所有輸入的 SoftModule 都出現在輸出中
2. **輪廓內**: 每個模組 `0 ≤ x`, `0 ≤ y`, `x + w ≤ ChipW`, `y + h ≤ ChipH`
3. **面積**: `w × h ≥ min_area`
4. **長寬比**: `0.5 ≤ h/w ≤ 2.0`
5. **無重疊**: 任意兩模組（含固定模組）不重疊（邊界貼齊不算重疊）
6. **固定模組一致**: 固定模組的位置與輸入完全一致（雖不需輸出，但會用於重疊檢查）

## Scorer Parsing Logic

`scorer/lib/floorplanning.py` 解析流程：
1. 讀取所有非空行，每行 tokenize
2. 若首行首 token 為 `wirelength`（不區分大小寫）→ 讀取下一 token 為 `self_reported` 值
3. 讀 `NumSoftModules <n>`
4. 讀 n 行，每行 5 個 token: `name x y w h`

## HPWL Calculation

scorer 使用以下公式獨立計算 wirelength：
```
centers[name] = (x + w//2, y + h//2)  # 整數除法
for m1, m2, weight in nets:
    (x1, y1) = centers[m1]
    (x2, y2) = centers[m2]
    wirelength += weight * (abs(x1-x2) + abs(y1-y2))
```
