# Model Implementation Diff and Score Comparison

資料來源：

- 分數以各 experiment 目錄下的 `score.csv` / `score.md` 為準；例外是 `002-floorplanning/qwen3.6-27b-autoround`，依實驗紀錄使用 `RESULT.md` 的 v5 數字，因為當時只更新 Result。
- Min / Reference / Max 取自各題 `README.md`。
- 實作差異主要根據各 model 的 `main.cpp`、`RESULT.md` 與程式註解整理。
- 注意：`problems/002-floorplanning/experiments/qwen3.6-27b-autoround/RESULT.md` 記錄的 v5 數字與目前 `score.csv` 不一致；本文件的 P2 qwen 比較採用 `RESULT.md`，其餘採用 scorer 產生的 `score.csv` / `score.md`。

## Overall Summary

| problem | available models | metric | headline result |
|---|---|---|---|
| 001-partitioning | opus / gpt / qwen | cut size, lower is better | opus public-case 總和最低；gpt 達成最多 Min；三者皆合法。 |
| 002-floorplanning | opus / gpt / qwen | wirelength, lower is better | opus 4/4 public case 全部最佳，且 public1/public4 低於 Min。 |
| 003-global-placement | opus only | HPWL, lower is better | opus 3/3 合法，全數優於 Reference，public1/public3 優於 Min。 |

---

## 001 Partitioning

### Problem / Goal / Constraints

- **題目描述**：把所有 cells 分配到 `DieA` / `DieB`。兩個 die 使用不同 Tech，同一個 LibCell 在不同 Tech 下可能有不同面積。
- **目標**：最小化跨 die nets 數量，也就是 **cut size**。越小越好。
- **合法性限制**：每個 cell 必須且只能被分到一個 die；DieA / DieB 的使用面積不能超過各自 utilization 上限；scorer 以輸出分區重算 cut size。
- **實作重點**：這題本質是 area-constrained hypergraph bipartitioning，所以差異主要在 hypergraph 資料結構、初始分割、FM refinement、multilevel coarsening/uncoarsening，以及大測資 time budget 如何分配。

### Implementation Differences

| model | data model / parser | initialization | main optimization | multilevel strategy | legality / fallback | parallelism / runtime | observed result |
|---|---|---|---|---|---|---|---|
| claude-opus-4-8 | 約 646 行，偏精簡。用 mmap-like whole-file buffer + `string_view` tokenizer，cell/net name 用 FNV hash intern；hypergraph 存成 net->cell 與 cell->net CSR，少做字串複製。 | 依 cell 面積排序做 greedy placement；優先放進 capacity 壓力較低的 die，seed 版本會有少量 randomized flip；若超 capacity 會 repair feasibility。 | 核心是 area-constrained FM。用 bucket gain 結構做近 O(1) extract-max / gain update；每 pass 記錄 cumulative gain，最後 rollback 到 best prefix。 | 小 case (`<3000` cells) 直接 flat multi-start FM；大 case 做 heavy-edge / first-choice matching coarsen，coarsest 多起點 FM，再逐層 uncoarsen + FM refine。V-cycle 反覆跑到 deadline 或 stall。 | 寫出前重算 used area / cut；若 multilevel 找不到合法解，會退回 deterministic greedy + FM safety fallback。 | OpenMP parallel multi-start；約 285s internal deadline；V-cycle 是 deadline/stall-bounded。 | public2/public3/public6 最佳，public-case 總和最低；6/6 public 都優於 Reference。 |
| gpt-5.5 | 約 1091 行，結構較大。用 `FastScanner`、`std::string` name map、CSR net/cell adjacency；狀態物件保存 side、countA/countB、used area、cut。 | 多種 restart seed。先建合法初始分割，再做 repair；有依 testcase Min target 的 early-stop。 | 除 FM 外，多了 positive-gain cleanup、bounded A/B pair-swap polish、以及 anneal pass。pair-swap 會估兩個 cell 對 cut 的 delta，專門處理單點 FM 卡住的 capacity-tight local minima。 | flat solver 和 multilevel solver 並行思路：heavy-edge matching 建 coarse graph，coarse 解 project 回 fine；每層再跑 positive cleanup / FM / swaps / anneal。 | 每個階段都 recompute counts 確認 legality；若 parallel 找到 target cut 會停後續 restart。 | OpenMP dynamic multi-start；thread count 依 size 調整，小/中 case 最多 8 threads，大 case 降到 4-6 threads 避免記憶體爆。 | public1/public4/public5 最佳；4/6 public 達到 Min，達成 Min 數最多；public6 是主要失分點。 |
| qwen3.6-27b-autoround | 約 938 行。tokenizer 會先把全部 token 存成 `vector<string>`，資料結構較直接；State 內放 CSR、countA/countB、area、side。 | coarsest / flat start 有多種 strategy：area descending greedy、random assignment、alternating assignment 等。 | FM 為主，包含 priority queue gain selection、full FM passes、positive-gain moves；v2 強調 O(1) delta update。 | random-order heavy-edge matching，目標提高 matching coverage；最多 25 levels。coarsest 做 multi-start FM，uncoarsen 時 coarse level heavy refinement，接近 fine level 時少量或 projection-only refine。若 coarsening ratio 不夠好，退回 flat FM。 | 最後重算 accurate cut 與 used area；若沒有合法結果，fallback 到 greedy。 | OpenMP dynamic multi-start；大 case 依估計每 start 成本自動決定 starts/time budget。 | 6/6 public 合法且 at/below Reference；public3 達到 Min；但沒有單項最佳。 |

### Score Comparison

Lower is better. The `best` column marks the best model score in that testcase.

| testcase | Min | Reference | opus | gpt | qwen | best |
|---|---:|---:|---:|---:|---:|---|
| public1 | 104 | 193 | 107 | **104** | 193 | gpt |
| public2 | 816 | 3,666 | **858** | 880 | 3,353 | opus |
| public3 | 1,762 | 7,092 | **1,128** | 1,424 | 1,699 | opus |
| public4 | 982 | 2,265 | 1,797 | **917** | 1,780 | gpt |
| public5 | 297 | 1,669 | 702 | **289** | 741 | gpt |
| public6 | 5,159 | 10,281 | **4,836** | 11,266 | 6,064 | opus |

| model | valid public cases | cases <= Min | cases <= Reference | public metric sum | case wins |
|---|---:|---:|---:|---:|---:|
| claude-opus-4-8 | 6/6 | 2/6 | 6/6 | **9,428** | 3 |
| gpt-5.5 | 6/6 | **4/6** | 5/6 | 14,880 | 3 |
| qwen3.6-27b-autoround | 6/6 | 1/6 | 6/6 | 13,830 | 0 |

### Interpretation

- opus 的 multilevel V-cycle 對大圖最穩，尤其 public6 拉開很大差距，所以總和最好。
- gpt 的 local polish 很有效，拿下 public1/public4/public5，也最常達到 Min，但 public6 是主要失分點。
- qwen 整體合法且普遍比 Reference 好，但搜索強度或 refinement 品質不足以拿下單項最佳。

---

## 002 Floorplanning

### Problem / Goal / Constraints

- **題目描述**：在固定晶片輪廓 `ChipSize W H` 內擺放 soft modules 和 fixed modules。soft module 只有面積固定，長寬可選；fixed module 的位置和尺寸固定。
- **目標**：最小化 weighted wirelength。scorer 以 module center 計算每條 two-pin net 的 Manhattan distance，再乘 net weight。越小越好。
- **合法性限制**：所有 soft modules 必須在 chip outline 內，不能互相重疊，也不能與 fixed modules 重疊；soft module 面積需滿足輸入 area，且 aspect ratio 約束需符合題目規格。
- **實作重點**：這題難點是「合法擺放」和「線長最佳化」同時處理。三個 model 都採 grid-free rectangle geometry，但在 initial packing、local move set、shape search、SA / median descent / compaction 的比例差很多。

### Implementation Differences

| model | data model / geometry | shape handling / init | local optimization | stochastic search | legality handling | parallelism / runtime | observed result |
|---|---|---|---|---|---|---|---|
| claude-opus-4-8 | 約 745 行。`Problem` 是 read-only，`State` per chain 保存 x/y/w/h/cx/cy/wl；不用 occupancy grid，所有 overlap 都用 rectangle pair 檢查與 contact-point candidates。 | 對 soft module 產生多個合法 `(w,h)` candidates，aspect ratio 0.5-2；constructive bottom-left packing，嘗試 area order、max-dim order、shuffled order 等，確保每個 chain 有合法起點。 | weighted-median coordinate descent 是主力：對每個 module 用 incident nets 的鄰居 center 找 L1 weighted median，再用 contact points 找合法最近位置；另有 reshape sweep 在同 center 附近換 shape。 | SA move set 包含 random displacement、median nudge、reshape、swap；swap 會計算 incident net delta 並用 Metropolis 接受。整體是「先合法 packing，再用 median descent 強力收斂，SA 做跳脫」。 | `legalAt()` 每次檢查 outline + all-rectangle overlap；只接受合法 move，所以沒有後處理修復成本。 | OpenMP parallel multi-start，固定 restart budget；RESULT 顯示約 245-322s。 | 4/4 public 全部最佳；public1/public4 低於 Min；全數大幅優於 Reference。 |
| gpt-5.5 | 約 717 行。`Problem/Module/Rect/Saved` 結構較簡單，同樣採 grid-free rectangles；有 soft/fixed id lists 和 incident nets。 | shape enumeration 偏保守，依 area/square 附近產生 candidates；constructive packing 後保存最佳合法 rects。 | median descent + compaction + local search。相較 opus，move set 和 intensification 較少，偏向穩定改善而非大量複合 move。 | 有時間上限下的 SA/local search，保存 best legal；但沒有 qwen 那麼多 perturb/recover，也沒有 opus 那麼完整的 contact-point median relocation。 | 所有候選 move 先測 `intersects` / `containsRect`，合法才採用；570s limit。 | OpenMP 可用；目前 scorer artifact 全合法。 | 4/4 public 都優於 Reference；public4 低於 Min；但每個 public 都輸 opus。 |
| qwen3.6-27b-autoround | 約 1180 行。全域 `gModules` + `thread_local modulesTLS`，每條 chain 複製 modules 隔離狀態；資料結構較工程化但複雜。 | shape generation 包含 square fallback；initialization 有 constructive packing，也有 neighbor-centroid init，讓部分 chains 先靠近 net 鄰居。 | local improvement 很多：incremental median descent、4-direction compaction、reverse-order compaction、post-compaction median、shape re-trial。 | move set 最豐富：adaptive SA、recency-based module selection、swap positions、swap+reshape、swap+compact、perturb & recover。理論探索力最強，但也更依賴參數。 | 每次 move 用 `legalPlacement()` 檢查 outline/area/aspect/overlap；最後再全域合法性檢查與 best restore。 | `std::thread` multi-chain，case-dependent chains，575s deadline；用 mutex 合併 global best。 | 依 `RESULT.md` v5，public1/public2/public4 優於 Reference，public3 高於 Reference；總和接近 gpt，但仍落後 opus/gpt。 |

### Score Comparison

Lower is better. The `best` column marks the best model score in that testcase.

| testcase | Min | Reference | opus | gpt | qwen | best |
|---|---:|---:|---:|---:|---:|---|
| public1 | 161,609,972 | 239,984,392 | **158,054,586** | 175,276,050 | 168,541,446 | opus |
| public2 | 20,966,863 | 38,494,434 | **24,136,170** | 26,308,875 | 31,617,508 | opus |
| public3 | 1,856,276 | 2,621,582 | **1,918,931** | 2,029,090 | 3,084,565 | opus |
| public4 | 63,024,850 | 137,686,350 | **62,396,925** | 62,979,325 | 67,377,025 | opus |

| model | valid public cases | cases <= Min | cases <= Reference | public metric sum | case wins |
|---|---:|---:|---:|---:|---:|
| claude-opus-4-8 | 4/4 | **2/4** | 4/4 | **246,506,612** | **4** |
| gpt-5.5 | 4/4 | 1/4 | 4/4 | 266,593,340 | 0 |
| qwen3.6-27b-autoround | 4/4 | 0/4 | 3/4 | 270,620,544 | 0 |

### Interpretation

- opus 的 weighted-median + reshape + SA 組合最穩，四個 public 都是最低。
- gpt 和 opus 的方向相近，但 local search 強度或 restart 品質略弱；仍然是穩定第二名。
- qwen 的策略很多，理論上探索空間大；依 `RESULT.md` v5，public1/public2/public4 比目前 `score.csv` 舊數字更好，總和已接近 gpt，但 public3 仍是主要弱點。

---

## 003 Global Placement

Only `claude-opus-4-8` is currently available.

### Problem / Goal / Constraints

- **題目描述**：讀入 Bookshelf `.aux`，再解析 `.nodes / .nets / .pl / .scl / .wts`，輸出每個 movable cell 的 global placement 座標。
- **目標**：最小化 HPWL。此 repo scorer 使用 unweighted HPWL，pin global coordinate 依 lower-left coordinate + pin offset 計算。
- **合法性限制**：所有 movable cells 要有輸出；fixed terminals 不能移動；座標要在 core 內。scorer 允許 global placement 重疊，但會用 row legalizer 做 anti-collapse health check，平均位移必須 `<= 0.05 * min(coreW, coreH)`，不能把 cell 全堆在一起。
- **實作重點**：這題不是 final detailed placement，而是要在「可低位移 legalization」前提下壓低 HPWL。因此 implementation 差異會集中在 analytical placement objective、density penalty、row/legalizer proxy 與 parallel gradient。

### Implementation Summary

| model | data model / parser | placement initialization | objective / optimization | density / legality strategy | parallelism / runtime | observed result |
|---|---|---|---|---|---|---|
| claude-opus-4-8 | 約 893 行，自包含 analytical placer。直接解析 `.aux/.nodes/.nets/.pl/.scl`；cells 用 SoA (`CW/CH/CX/CY/CFIX`)，nets/pins 用 CSR (`NET_PTR/PIN_CELL/PIN_POX/PIN_POY`)；pin offset 轉成 relative-to-center，方便 HPWL/LSE 計算。 | L1 先做 uniform constructive spread，把 movable cells 均勻放在 core/rows 上，作為 guaranteed-legal fallback。這避免一開始就 collapse。 | L2 用 LSE wirelength surrogate 近似 HPWL，Adam 更新 movable centers；先 WL-dominant 讓 connected cells 聚集，再逐步增加 density lambda。過程中保留 scorer-HPWL 最低且 legalizer displacement 合格的 snapshot。 | 使用 bell-shaped bin-density penalty，不是硬 legalization。程式內 port 了一個 scorer-like row Tetris legalizer，週期性計算 normalized avgDisp；lambda ramp 到 legal 後 freeze，避免過度均勻化傷害 HPWL。coarse bins 是關鍵，太細會破壞 wirelength arrangement。 | OpenMP parallel objective/gradient；thread-local partial gradients + fixed-order reduction，確保 deterministic；約 560s guard，但實測 15-72s。 | 3/3 合法，全數優於 Reference；public1/public3 低於 Min，public2 只比 Min 高約 8.2%。 |

### Score Comparison

Lower is better.

| testcase | Min | Reference | opus HPWL | valid | note |
|---|---:|---:|---:|:---:|---|
| public1 | 59,788,412 | 87,987,694 | **46,566,011** | OK | beats Min and Reference |
| public2 | 10,530,075 | 18,642,174 | **11,389,895** | OK | beats Reference, +8.2% over Min |
| public3 | 395,131,978 | 750,902,922 | **358,399,034** | OK | beats Min and Reference |

| model | valid public cases | cases <= Min | cases <= Reference | public metric sum |
|---|---:|---:|---:|---:|
| claude-opus-4-8 | 3/3 | 2/3 | 3/3 | 416,354,940 |

### Interpretation

- opus 的重點不是做完整 legalization，而是維持 global placement 可被 scorer legalizer 低位移攤開；這和 scorer 的 anti-collapse gate 對齊。
- coarse bin density 是關鍵：程式註解指出太細的 bins 會破壞 WL arrangement，coarse bins 保留 connectivity 聚集效果，public3 從高 HPWL 降到 358M。
- 目前缺 gpt/qwen 解，因此 P3 還不能做跨 model 排名。

---

## Cross-Problem Takeaways

1. **opus**：整體最穩。P1 public-case 總和最低，P2 完全領先，P3 目前唯一且品質好。共同特徵是實作較精簡但 scorer convention 對齊得很準，且每題都有明確 fallback 或 legality guard。
2. **gpt**：局部搜尋能力強。P1 拿下最多 Min 和 3 個 case wins，P2 穩定第二。弱點是大 case 或 memory/time-sensitive case 可能出現品質崩落，例如 P1 public6。
3. **qwen**：策略豐富但目前 artifact 不夠穩。P1 合法且可打 Reference，但沒有單項最佳；P2 的 `RESULT.md` 與 `score.csv` 不一致，應先重新 scorer 驗證後再判斷 v5 是否真的改善。
