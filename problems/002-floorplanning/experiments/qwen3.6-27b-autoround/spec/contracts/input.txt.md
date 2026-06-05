# Input Format Contract: `.txt`

## Overview

輸入檔為純文字格式，使用空白（空白/製表符）分隔欄位，空行作為區段分隔。

## Structure

```
ChipSize <W> <H>

NumSoftModules <n>
SoftModule <name_1> <area_1>
SoftModule <name_2> <area_2>
...

NumFixedModules <m>
FixedModule <name_1> <x_1> <y_1> <w_1> <h_1>
FixedModule <name_2> <x_2> <y_2> <w_2> <h_2>
...

NumNets <k>
Net <module_A_1> <module_B_1> <weight_1>
Net <module_A_2> <module_B_2> <weight_2>
...
```

## Field Specifications

### ChipSize
| Position | Field | Type | Constraint |
|----------|-------|------|------------|
| 1 | `W` | `int` | `> 0` |
| 2 | `H` | `int` | `> 0` |

### SoftModule
| Position | Field | Type | Constraint |
|----------|-------|------|------------|
| 1 | `name` | `string` | 唯一；字母數字 + 底線 |
| 2 | `area` | `int` | `> 0`；為 `w × h` 的最小值 |

### FixedModule
| Position | Field | Type | Constraint |
|----------|-------|------|------------|
| 1 | `name` | `string` | 唯一；字母數字 + 底線 |
| 2 | `x` | `int` | `≥ 0` |
| 3 | `y` | `int` | `≥ 0` |
| 4 | `w` | `int` | `> 0`；`x + w ≤ ChipSize.W` |
| 5 | `h` | `int` | `> 0`；`y + h ≤ ChipSize.H` |

### Net
| Position | Field | Type | Constraint |
|----------|-------|------|------------|
| 1 | `module_A` | `string` | 必須對應到某 SoftModule 或 FixedModule 的 name |
| 2 | `module_B` | `string` | 必須對應到某 SoftModule 或 FixedModule 的 name；≠ module_A |
| 3 | `weight` | `int` | `> 0` |

## Example

```
ChipSize 8 7

NumSoftModules 2
SoftModule GPU 25
SoftModule CPU 15

NumFixedModules 2
FixedModule PAD1 0 5 2 2
FixedModule FIXED1 5 0 3 2

NumNets 3
Net GPU CPU 20
Net GPU PAD1 10
Net CPU FIXED1 15
```

## Parsing Notes

- 空行可存在於區段之間，但非強制
- Tokenizer 依空白分隔（`line.split()` 即可）
- 所有數值為整數，無浮點數
- 模組名稱不區分大小寫（但建議保持一致）
