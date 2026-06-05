# Phase 1 Data Model: Fixed-outline Floorplanning（claude-opus-4-8）

> 實體、欄位、合法性規則與 HPWL 定義。約束來源為官方 `verify`（真理）與共用 baseline。

## 實體（Entities）

### Chip（晶片輪廓）
| 欄位 | 型別 | 說明 |
|------|------|------|
| W | int | 寬度，固定 |
| H | int | 高度，固定 |

- 不變量：所有模組須完全落在 `[0,W] × [0,H]`。

### SoftModule（可變形模組）
| 欄位 | 型別 | 說明 |
|------|------|------|
| name | string | 唯一名稱 |
| area | long | 給定最小面積（min area） |
| x, y | int | 左下角座標（求解輸出） |
| w, h | int | 寬高（求解輸出） |

- 不變量：
  - `w × h ≥ area`（**下限**，非等號）。
  - `w / h ∈ [0.5, 2.0]`（長寬比；等價 `2h ≥ w` 且 `w ≥ ceil(h/2)`，以整數保守判定）。
  - `x ≥ 0, y ≥ 0, x+w ≤ W, y+h ≤ H`。
  - 與任何其他模組不重疊。
- 衍生：中心 `cx = x + w/2`、`cy = y + h/2`（整數除法）。

### FixedModule（固定模組）
| 欄位 | 型別 | 說明 |
|------|------|------|
| name | string | 唯一名稱 |
| x, y, w, h | int | 位置與尺寸，**輸入給定、不可更動** |

- 不變量：輸出/計算過程中座標尺寸恆等於輸入；彼此及與 soft 不重疊（輸入保證 fixed 間合法）。
- 衍生：中心同上。

### Net（連線）
| 欄位 | 型別 | 說明 |
|------|------|------|
| a, b | Module ref | 兩端模組（可為 soft 或 fixed） |
| weight | int | 正整數權重 |

- 恰 2 端點。對 HPWL 的貢獻 `weight × (|cxA−cxB| + |cyA−cyB|)`。

### Floorplan（輸出解）
- 所有 soft module 的最終 `(x,y,w,h)` 集合 + 一個整數 `Wirelength`。
- fixed module **不寫入輸出**（verifier 由輸入取得 fixed 位置）。

## 關係（Relationships）
- Net 多對多參照 SoftModule / FixedModule（每 net 2 參照）。
- Floorplan 一對一對應一份輸入測資。

## 合法性規則彙整（verifier 判定）
| 規則 | 來源訊息 | 判定 |
|------|----------|------|
| 輪廓內 | （邊界） | `0≤x, 0≤y, x+w≤W, y+h≤H` |
| 不重疊 | `overlaps with fixed/soft module` | 任兩矩形交集面積 = 0 |
| 面積下限 | `area ... greater than the min area` | `w×h ≥ area` |
| 長寬比 | `aspect ratio ... range from 0.5 to 2` | `0.5 ≤ w/h ≤ 2.0` |
| soft 完整 | `Missing/Duplicated Soft Module` | 輸出含全部 soft、不重不漏 |
| 線長一致 | `Wrong Wirelength` | 首行值 = verifier 計算之整數 HPWL |
| 格式 | `Wrong Format / Unknown Content / Missing Tag` | 依輸出格式契約 |

## HPWL 計算（與輸出 Wirelength 必須一致）
```
for each net (A,B,weight):
    cxA = A.x + A.w/2;  cyA = A.y + A.h/2     // 整數除法
    cxB = B.x + B.w/2;  cyB = B.y + B.h/2
    HPWL += weight * ( |cxA-cxB| + |cyA-cyB| )
```
- 全程整數運算；輸出首行 `Wirelength <HPWL>`。

## 狀態轉移（求解流程中的解狀態）
`Parsed → GloballyPlaced（中心目標）→ Shaped（指派 w,h）→ Legalized（無重疊、合法）→ Refined（SA 最佳）→ Output`
- 僅 `Legalized` 與 `Refined` 為合法可輸出狀態；`Refined` 全程保留歷史最佳合法解以防退化。
