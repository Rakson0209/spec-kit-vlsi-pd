# Data Model: 固定輪廓平面規劃

## Entity: Chip (晶片輪廓)

| Field | Type | Description |
|-------|------|-------------|
| `width` | `int` | 晶片寬度（像素） |
| `height` | `int` | 晶片高度（像素） |

**Constraints**: `width > 0`, `height > 0`

---

## Entity: Module (模組)

### SoftModule（軟模組）

| Field | Type | Description |
|-------|------|-------------|
| `name` | `string` | 模組名稱（唯一標識符） |
| `min_area` | `int` | 最小面積（w × h ≥ min_area） |
| `x` | `int` | 左上角 x 座標（輸出時決定） |
| `y` | `int` | 左上角 y 座標（輸出時決定） |
| `width` | `int` | 模組寬度（輸出時決定） |
| `height` | `int` | 模組高度（輸出時決定） |

**Constraints**:
- `x ≥ 0`, `y ≥ 0`
- `x + width ≤ chip.width`
- `y + height ≤ chip.height`
- `width × height ≥ min_area`
- `0.5 ≤ height / width ≤ 2.0`
- `width > 0`, `height > 0`

**Shape Candidates**: 在 min_area 與 aspect ratio 約束下，可能的 (w, h) 組合：
- `min_width = ceil(sqrt(min_area / 2.0))`
- `max_width = floor(sqrt(2.0 * min_area))`
- 在此範圍內選取離散的 width 值，height = ceil(min_area / width)

**Center**: `cx = x + width / 2`, `cy = y + height / 2`（整數除法）

### FixedModule（硬模組）

| Field | Type | Description |
|-------|------|-------------|
| `name` | `string` | 模組名稱（唯一標識符） |
| `x` | `int` | 固定 x 座標 |
| `y` | `int` | 固定 y 座標 |
| `width` | `int` | 固定寬度 |
| `height` | `int` | 固定高度 |

**Constraints**: 所有欄位從輸入檔讀取後不可改變

**Center**: `cx = x + width / 2`, `cy = y + height / 2`（整數除法）

---

## Entity: Net（連線）

| Field | Type | Description |
|-------|------|-------------|
| `module_a` | `string` | 端點 A 模組名稱（可為軟模組或硬模組） |
| `module_b` | `string` | 端點 B 模組名稱（可為軟模組或硬模組） |
| `weight` | `int` | 連線權重 |

**HPWL Calculation**: `weight × (|cx_a - cx_b| + |cy_a - cy_b|)`

**Total Wirelength**: `Σ (所有 net 的 HPWL)`

---

## Entity: SequencePair (序列對)

Sequence-pair 是 floorplan 的編碼方式，包含兩個相同長度的排列序列：

| Field | Type | Description |
|-------|------|-------------|
| `sigma_plus` | `vector<string>` | 水平約束序列（σ⁺） |
| `sigma_minus` | `vector<string>` | 垂直約束序列（σ⁻） |

**Decoding**: 從 σ⁺ 和 σ⁻ 建立 constraint graph，解出每個模組的相對位置關係，再依 min_area 和 aspect ratio 算出最終 (x, y, w, h)。

**Rules**:
- 若 A 在 B 之前出現在 σ⁺ 和 σ⁻ → A 在 B 左方（水平約束）
- 若 A 在 B 之前出現在 σ⁺ 但後於 σ⁻ → A 在 B 上方（垂直約束）
- 若 A 在 B 之前出現在 σ⁻ 但後於 σ⁺ → A 在 B 下方
- 若 A 在 B 後出現在 σ⁺ 和 σ⁻ → A 在 B 右方

---

## Entity: Floorplan (平面配置結果)

| Field | Type | Description |
|-------|------|-------------|
| `modules` | `vector<Module>` | 所有模組（軟+硬）的最終配置 |
| `wirelength` | `int` | 總加權 HPWL |

**Validity**: 所有模組不重疊、在輪廓內、滿足 area/aspect ratio 約束

---

## Relationships

```
Chip (1) ── contains ── (N) Modules
Module (N) ── connected_by ── (M) Nets
SequencePair (1) ── decodes_to ── (1) Floorplan
```
