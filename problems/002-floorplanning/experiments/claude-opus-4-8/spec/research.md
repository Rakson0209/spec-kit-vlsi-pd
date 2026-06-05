# Phase 0 Research: Fixed-outline Floorplanning（claude-opus-4-8）

> 依憲章原則 VI：本檔須在任何實作前完成。記錄 (1) baseline 門檻、(2) 候選演算法調研與取捨、
> (3) 選定有潛力**超越** baseline 的方案。資料來源僅限共用題目資產：`reference/spec.pdf`、
> 共用 baseline `reference/src/main.cpp`、官方 `benchmark/verifier/verify`、`benchmark/testcase/`。

---

## 1. 要打敗的門檻（Baseline Metrics）

最佳化指標：**加權 wirelength（HPWL，越小越好）**。下表 **Min 為要超越的門檻**（已知最佳），
`Reference` 為人類參考解（baseline，即 `reference/src/main.cpp`）實測，`Max` 為零分門檻。

| testcase | **目標 Min（要 ≤）** | Reference（baseline） | Reference runtime | Max（零分） |
|----------|--------------------:|----------------------:|------------------:|------------:|
| public1  | **161,609,972**     | 239,984,392           | 581.41s           | 349,768,634 |
| public2  | **20,966,863**      | 38,494,434            | 45.86s            | 41,569,628  |
| public3  | **1,856,276**       | 2,621,582             | 111.55s           | 5,045,921   |
| public4  | **63,024,850**      | 137,686,350           | 285.35s           | 201,625,050 |

**門檻落差觀察**：Reference 距 Min 仍有 32%–54% 差距（public1 需再降 ~33%、public4 需再降 ~54%）。
要超越 Min 不能只是「微調 baseline」，須從演算法層面改寫。

### 測資規模（決定演算法可行性的關鍵）

| testcase | ChipSize (W×H) | NumSoft | NumFixed | NumNets |
|----------|---------------:|--------:|---------:|--------:|
| sample   | 8 × 7          | 2       | 2        | 3       |
| public1  | 11267 × 10450  | 15      | 5        | 45      |
| public2  | 2300 × 2300    | 16      | 8        | 39      |
| public3  | 2500 × 3000    | 28      | 14       | 108     |
| public4  | 4995 × 4407    | 20      | 8        | 47      |

**第一性原理關鍵洞察**：soft module 數量 **N ≤ 28**，極小。任何「每次操作正比於 N²」的演算法都近乎免費
（28² = 784）；真正昂貴的是 baseline 那種「正比於晶片面積」的網格操作（public1 達 1.18 億格）。

---

## 2. 合法性約束（取自官方 verifier，為真理）

由 `verify` 二進位字串與共用 baseline 交叉確認：

1. **輪廓內**：每模組 `0 ≤ x`, `0 ≤ y`, `x+w ≤ W`, `y+h ≤ H`；座標為整數（verifier 幾何 `Rectangle(long×4)`）。
2. **不重疊**：任兩模組矩形交集面積為 0（錯誤訊息 `overlaps with fixed/soft module`）。
3. **面積下限**：soft module `w×h ≥ 給定 area`（訊息：`area ... should be greater than the min area`）——**是下限不是等號**。
4. **長寬比**：soft module `w/h ∈ [0.5, 2.0]`（訊息：`aspect ratio ... should be in range from 0.5 to 2`）。
5. **Fixed 不動**：fixed module 座標尺寸維持輸入值。
6. **Wirelength 一致**：輸出首行 `Wirelength <值>` 須等於 verifier 計算之**非負整數** HPWL（否則 `Wrong Wirelength!`）。

### HPWL 精確定義

每條 net 恰連 2 個模組（格式 `Net <A> <B> <weight>`）。以模組**中心**計，整數座標：

```
cx = x + w/2   (整數除法), cy = y + h/2
HPWL = Σ_net  weight × ( |cxA − cxB| + |cyA − cyB| )
```

與共用 baseline `HPWL()` 一致（baseline 為通過 verifier 的參考解，故此式即 verifier 之定義）。
**實作須以同一整數中心公式計算並輸出**，確保 `Wirelength` 比對通過。

---

## 3. Baseline 弱點分析（為何只到 Reference、打不到 Min）

剖析 `reference/src/main.cpp`：

- **全晶片布林網格** `vector<vector<bool>>(H, W)`：public1 需 ~118M cell（~118MB）；`set_grid`/`check_placed`/
  `compact` 皆 O(模組面積)，單次 SA 移動成本可達數百萬次格存取 → **迭代次數被嚴重壓低**，搜尋不充分。
- **cost 含面積項**（`alpha=0.5` 對 total_area 加權）：固定輪廓下只要合法，面積與目標無關；
  把一半最佳化壓力浪費在無關量 → 線長收斂變差。
- **移動算子受網格束縛**：move 只能往「上 / 右」找隨機空格，落子受網格佔用限制，難做大跨度的線長改善。
- **無全域線長導向初始化**：初始擺放是面積排序的貪婪落子，與「連線近鄰相聚」無關，起點差。

結論：baseline 的瓶頸是「資料結構（網格）+ 目標（含面積）+ 缺乏線長導向」三重拖累。要超越 Min 須三者一起改。

---

## 4. 候選演算法調研與取捨

| 方案 | 核心想法 | 對本題優點 | 缺點 / 風險 | 預期品質 |
|------|----------|-----------|------------|---------|
| **A. Sequence Pair + SA** | 以序列對表示拓樸，packing 壓縮成無重疊 | 經典、保證無重疊 | fixed module 在任意絕對位置，SP 壓縮模型難自然容納；壓縮偏向角落，與「線長」目標衝突；表示/解碼成本高 | 中 |
| **B. B*-tree + SA** | 二元樹表示緊鄰關係 | 緊湊、無重疊 | 同 A，fixed 模組與固定輪廓不易表達；偏面積最佳化 | 中 |
| **C. 純網格 SA（baseline 類）** | 全晶片網格 + 移動算子 | 實作直觀 | 即 baseline，已知打不到 Min | 低（=Reference） |
| **D. 無網格 SA（絕對座標 + O(N²) 重疊檢查，HPWL-only cost）** | 模組存絕對矩形，移動/換形/交換，O(N²) 檢查 | N≤28 使每步近免費 → 可跑數百萬步；cost 純線長 | 隨機起點對 public1 較激進 Min 可能收斂不足 | 高 |
| **E. 力導向/解析式全域擺放 → 合法化 → 無網格 SA 精修（混合）** | 先用線長導向把中心拉到近最佳相對位置（fixed 當錨點），再合法化去重疊，最後 SA 精修 | 直擊真實目標（線長）；起點接近最佳；SA 只需微調 → 最穩定超越 Min | 合法化需穩健（避免去重疊後又超界/破壞 AR） | 最高 |

### 為何不選 A/B（SP / B*-tree）

這兩者是**面積導向**的 floorplan 表示，強項是把模組壓成最小外接矩形。但本題是**固定輪廓 + 線長唯一目標**，
且 fixed module 已釘死在任意絕對座標——壓縮式表示難以表達「繞開固定障礙、把連線近鄰拉近」的需求，
解碼也比直接操作絕對座標昂貴。對「最小化線長」而言是繞遠路。

### 為何 D 比 C 強

把 baseline 的網格換成「絕對座標矩形 + O(N²) 重疊檢查」，單步成本從 O(晶片面積) 驟降到 O(N²)；
再把 cost 改為純 HPWL。光這兩點就能讓相同時間內的有效迭代數提升數個量級，足以明顯優於 Reference。

---

## 5. 選定方案（Decision）

**Decision**：採 **方案 E ＝ 力導向/解析式全域擺放（線長導向）→ 形狀與長寬比指派 → 貪婪合法化
→ HPWL-only 無網格模擬退火精修**，並以**保證合法的建構式落子**作為 fallback。

**Rationale（第一性原理）**：
1. HPWL 只由「模組中心相對位置」決定 → 先用力導向/解析式直接把高權重相連模組拉近（fixed 模組為固定錨點），
   取得**近最佳相對佈局**，這是 baseline 從未逼近、也是打到 Min 的關鍵。
2. N≤28 使合法化後的 **SA 精修**極廉價（O(N²) 重疊檢查、無網格），可在 600s 內跑數百萬步，
   負責去合法化殘留的次佳、跳出局部最優、並在 AR∈[0.5,2]、面積≥area 限制內微調形狀以再縮線長。
3. cost 全程**只含 HPWL**（丟棄面積項），最佳化壓力 100% 對齊評測指標。

**Algorithm pipeline**：
1. **Parse**：讀入 chip、soft（area）、fixed（x,y,w,h）、nets（A,B,weight）。
2. **Global placement（線長導向）**：將每個 soft module 視為點（中心），以加權連線最小化目標做
   力導向鬆弛（朝相連模組加權質心移動）/ 或解析式二次線長求解，fixed 模組為固定錨。得到各 soft 的目標中心。
3. **Shape/AR 指派**：為每個 soft module 選 `w×h ≥ area` 且 `w/h ∈ [0.5,2]` 的整數形狀（預設近正方、
   可依鄰居分佈微調長寬以縮線長）。
4. **Legalization（合法化）**：把目標中心轉成不重疊、在輪廓內的整數矩形（依目標位置排序的貪婪落子 /
   推擠）；**fallback**：若失敗，用面積排序的建構式 shelf/skyline 落子確保 100% 合法。
5. **SA refinement**：算子 = 平移 / 交換 / 換形 / 朝連線質心拉近；cost = HPWL；O(N²) 重疊檢查；
   Metropolis 接受；保留歷史最佳合法解；計時 ≤ ~580s 留輸出餘裕。
6. **Output**：以整數中心公式算 HPWL，寫 `Wirelength` 首行 + 各 soft module `name x y w h`。

**Alternatives considered**：方案 A/B（SP、B*-tree）因面積導向、難容固定障礙而否決；方案 C（網格 SA）即 baseline，
已知止於 Reference；方案 D（純無網格 SA）為 E 的子集，作為 E 全域擺放失效時的退化路徑保留。

**如何驗證有超越**：對 public1~4 跑完，記錄各自 HPWL 與 runtime，於 `RESULT.md` 對照 Min / Reference / Max。
目標 4 份全 ≤ Min；任一未達則須 < Reference 且 < Max，並分析差距原因（憲章原則 VI）。

---

## 6. 風險與緩解

| 風險 | 影響 | 緩解 |
|------|------|------|
| 合法化去重疊後破壞輪廓/AR/面積限制 → verifier fail | SC-001 不過 | 合法化每步即時檢查四項約束；保留建構式 fallback 保證合法 |
| 輸出 Wirelength 與 verifier 不一致 | `Wrong Wirelength!` | 以與 baseline 相同的整數中心公式計算後輸出 |
| public1 晶片大、Min 激進，SA 收斂不足 | 打不到 Min | 力導向初始化提供近最佳起點；放大迭代預算至接近 580s |
| 隨機性導致重現性差 | SC-005 | 固定 seed 並於 RESULT.md 記錄 |
| 整數座標捨入造成輕微重疊/超界 | verifier fail | 合法化與輸出階段以整數運算為準，邊界取保守捨入 |

> Phase 0 完成：門檻已記錄、候選已調研、方案已選定且論證可超越 baseline。可進入 Phase 1 設計。
