# Research: 固定輪廓平面規劃演算法分析

**Date**: 2026-06-04

## 1. 參考實作分析

### 1.1 整體架構
參考實作 (`reference/src/main.cpp`) 是一個單檔案 C++ 程式，使用模擬退火 (Simulated Annealing) 演算法。

### 1.2 資料結構
- **block**: 含 `name`, `x`, `y`, `width`, `height`, `min_area`, `shape`（候選尺寸清單）, `temp_x/y/width/height`（備用）
- **Grid**: `vector<vector<bool>>` 二維陣列，`true` = 已佔用
- **Net**: `vector<pair<int, pair<block*, block*>>>` 權重 + 兩個模組指標

### 1.3 初始放置
- 將軟模組依最小面積**由大到小**排序
- 對每個模組在 Grid 上尋找可用空間，嘗試 shape 清單中的各尺寸
- 使用 **compaction**（向上、向左緊密化）縮小空間浪費
- 若放置失敗，則重置所有已放置模組並重試（隨機取樣步進）

### 1.4 模擬退火參數
| 參數 | 值 | 說明 |
|---|---|---|
| `T` (initial) | 1000.0 | 初始溫度 |
| `T_MIN` | 1.0 | 最低溫度 |
| `T_DECAY` | 0.95 | 每輪溫度衰減率 |
| `REJECT_RATIO` | 0.95 | 拒收比上限，超過則停止 |
| `K` | 20 | `N = soft_module_count * K` |
| `DOUBLE_N` | `2N` | 每輪最大產生鄰解數 |
| timeout | 580 秒 | 執行時間上限 |

### 1.5 成本函數
```
cost = α × total_area + β × wirelength
α = 0.5, β = 0.5
```
面積項包含所有軟模組 + 硬模組的面積總和。

### 1.6 鄰域操作
| 操作 | 機率 | 說明 |
|---|---|---|
| Swap | 33% | 隨機交換兩個軟模組的位置與尺寸 |
| Move | 33% | 隨機移動一個軟模組（向上或向右） |
| Reshape | 34% | 隨機改變一個軟模組的形狀 |

### 1.7 不足之處
1. **初始放置**：隨機取樣步進 (`rand() % 11 + 20`) 在大尺寸晶片上效率低下
2. **成本函數**：α/β 固定為 0.5，未針對不同 testcase 調整
3. **溫度調度**：固定 0.95 衰減，未依問題規模調整初始溫度
4. **RNG**：使用 `rand()`（非可重現），未設 seed
5. **compaction**：只向上和向左，未考慮向下/向右
6. **reshape**：只在原位置嘗試，未移動位置配合新 shape

## 2. 計分器驗證規則 (`scorer/lib/floorplanning.py`)

### 合法性檢查
1. 所有軟模組必須有輸出
2. 每個軟模組在晶片輪廓內：`x ≥ 0, y ≥ 0, x+w ≤ W, y+h ≤ H`
3. 面積 `w × h ≥ min_area`
4. 長寬比 `0.5 ≤ h/w ≤ 2.0`
5. 所有模組（軟+硬）互不重疊
6. 所有 net 引用的模組名稱必須存在

### HPWL 計算
- 模組中心：`cx = x + w // 2`, `cy = y + h // 2`（**向下取整**）
- 加權 HPWL：`Σ weight × (|cx1-cx2| + |cy1-cy2|)`
- 參考值：`sample.txt` 的 HPWL = 215

## 3. 改良方向

### 3.1 初始放置
- **Grid-based 改為 sequential-pair 或 heuristic-based**：利用模組間 net 權重，將連接權重高的模組放得較近
- **Weighted center-of-gravity**：先計算每個模組的加權重心位置，再依此排序放置

### 3.2 成本函數調整
- 動態調整 α/β：初期偏重面積（快速縮小空間），後期偏重線長（微調位置）
- 或直接使用 `cost = wirelength`（因為面積已由 min_area 下限固定，shape 變化範圍有限）

### 3.3 溫度調度
- 依問題規模調整初始溫度：`T = HPWL_initial / ln(N)` 或類似公式
- 使用 adaptive cooling：當拒收比偏低時加速降溫

### 3.4 可重現性
- 使用 `std::mt19937` + 固定 seed（預設 42）
- 可透過命令行參數指定 seed

### 3.5 輸出格式
- 首行 `Wirelength <value>`，接 `NumSoftModules <n>`，再逐行輸出

## 4. 結論

參考實作的架構（模擬退火 + swap/move/reshape 三操作）是合理的基礎。
改良重點在：**更好的初始放置**、**動態成本函數**、**可重現 RNG**、**模組化程式碼**。
這些改良不需改變核心演算法，但可顯著提升解品質與除錯效率。
