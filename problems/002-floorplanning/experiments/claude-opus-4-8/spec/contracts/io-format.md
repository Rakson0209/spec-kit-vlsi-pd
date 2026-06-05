# Contract: 輸入 / 輸出格式（claude-opus-4-8）

> 介面契約。輸出格式須讓官方 `verify` 回傳 `[Success]`。

## 輸入 `.txt`

```
ChipSize <W> <H>

NumSoftModules <n>
SoftModule <name> <area>
... (n 行)

NumFixedModules <m>
FixedModule <name> <x> <y> <w> <h>
... (m 行)

NumNets <k>
Net <moduleA> <moduleB> <weight>
... (k 行)
```

- 區塊間以空行分隔（parser 須容忍空行）。
- 數值為整數。`area` 為 soft module 最小面積。
- Net 端點名稱必為已宣告之 soft 或 fixed module（題目保證）。

### 範例（sample.txt）
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

## 輸出 `.floorplan`

```
Wirelength <整數 HPWL>

NumSoftModules <n>
<name> <x> <y> <w> <h>
... (n 行，僅 soft modules)
```

- **第一行** `Wirelength <值>`：須等於 verifier 計算的整數 HPWL（見 data-model 的計算式）。
- 接一空行，再 `NumSoftModules <n>`。
- 每個 soft module 一行：`name x y w h`（整數，左下角座標 + 寬高）。
- **不輸出 fixed modules**。
- soft module 須全列、不重複、不遺漏（否則 `Missing/Duplicated Soft Module`）。

### 合法性自檢清單（送 verifier 前）
- [ ] 每 soft：`w×h ≥ area`、`0.5 ≤ w/h ≤ 2.0`、整數座標。
- [ ] 每模組在 `[0,W]×[0,H]` 內。
- [ ] 任兩模組（含 fixed）不重疊。
- [ ] `Wirelength` 首行 = 自算整數 HPWL。
- [ ] 輸出含全部 soft、格式正確。
