# Data Model: 固定輪廓平面規劃

**Date**: 2026-06-04

## Module（模組）

```cpp
struct Module {
    std::string name;         // 模組名稱
    int x, y;                 // 左下角座標
    int width, height;        // 實際尺寸
    int min_area;             // 最小面積要求
    bool fixed;               // 是否為硬模組（不可移動）

    // 候選 shape 清單：(width, height) 符合 h/w ∈ [0.5, 2] 且 w*h ≥ min_area
    std::vector<std::pair<int,int>> shapes;

    // 退火用備份
    int bk_x, bk_y, bk_w, bk_h;

    int center_x() const { return x + width / 2; }    // ⌊x + w/2⌋
    int center_y() const { return y + height / 2; }   // ⌊y + h/2⌋

    int area() const { return width * height; }
    float aspect_ratio() const { return (width > 0) ? (float)height / width : 0; }

    // 從 min_area 產生合法 shape 清單
    void generate_shapes();

    // 檢查當前尺寸是否合法（面積 + 長寬比）
    bool is_valid_shape() const;
};
```

### Shape 產生規則
- `min_width = ceil(sqrt(min_area / 2.0))`（h/w = 2 時的 w）
- `max_width = min(min_area, (int)floor(sqrt(min_area * 2.0)))`（h/w = 0.5 時的 w）
- 步進 `step = max(1, (max_width - min_width) / MAX_SHAPES)`
- 對每個 `w`，`h = ceil(min_area / w)`，檢查 `h/w ∈ [0.5, 2]`
- 限制 `MAX_SHAPES` 為 30（避免 shape 清單過長）

## Net（網路連接）

```cpp
struct Net {
    int weight;               // 連線權重
    Module* module_a;         // 模組 A 指標
    Module* module_b;         // 模組 B 指標
};
```

## Grid（格狀空間）

```cpp
class Grid {
    int chip_width, chip_height;
    std::vector<std::vector<bool>> occupied;  // occupied[y][x]

public:
    void set(int x, int y, int w, int h, bool val);
    bool can_place(int x, int y, int w, int h) const;
    std::pair<int,int> compact_up_left(int x, int y, int w, int h) const;
    std::pair<int,int> compact_down_right(int x, int y, int w, int h) const;
};
```

## Solution（解）

```cpp
struct Solution {
    std::vector<Module> soft_modules;
    std::vector<Module> fixed_modules;
    std::vector<Net> nets;
    int chip_width, chip_height;
    int wirelength;           // 加權 HPWL

    int compute_wirelength() const;
    bool is_valid() const;    // 檢查所有合法性
    void backup();            // 備份所有軟模組狀態
    void restore();           // 從備份還原
};
```

## Parser（輸入解析）

```
ChipSize W H
NumSoftModules n
  SoftModule name area
NumFixedModules n
  FixedModule name x y w h
NumNets n
  Net moduleA moduleB weight
```

輸出 `Parser::parse(filename)` → `Solution`

## Output（輸出格式）

```
Wirelength <value>

NumSoftModules <n>
name x y w h
...
```
