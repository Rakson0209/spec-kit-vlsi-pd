# claude-opus-4-8 — 003-global-placement 結果

- 模型 / 版本：claude-opus-4-8 (1M context)
- SDD 產物（本模型自產）：見 [./spec/](./spec/)（spec / plan / research / data-model / contracts / tasks）
- 編譯指令：`g++ -std=c++20 -O3 -fopenmp -pthread -static -o D:\FSecret\hw4.exe main.cpp`（單一自含 TU，無 Boost、無連結 `reference/obj`）
- 執行指令：`D:\FSecret\hw4.exe <input.aux> <output.gp.pl>`

## 計分（scorer/lib，純 Python，R6 唯一裁判）

| testcase | 合法(scorer) | HPWL | avgDisp | 執行時間 | Min | Reference | Max | vs Min | vs Ref |
|----------|:---:|------:|:---:|---:|------:|------:|------:|:---:|:---:|
| public1 | **OK** | 46,566,011 | 0.041× | 15.3s | 59,788,412 | 87,987,694 | 319,198,465 | **0.779×** | 0.529× |
| public2 | **OK** | 11,389,895 | 0.035× | 72.1s | 10,530,075 | 18,642,174 | 28,999,635 | 1.082× | **0.611×** |
| public3 | **OK** | 358,399,034 | 0.037× | 35.8s | 395,131,978 | 750,902,922 | 2,631,834,205 | **0.907×** | 0.477× |

- **3/3 合法**（無遺漏 / 超界 / 移動固定模組 / 塌縮；`avgDisp` 0.035–0.041× 皆 < 0.05 門檻，留 ~18–30% 餘裕 → SC-001/SC-006）。
- **全部 < Max**（SC-002），**全部 ≤ Reference 且嚴格 <**（SC-004）。
- **public1 與 public3 ≤ Min**（SC-003 命中 2/3）；public2 11.39M 勝 Reference、距 Min 僅 +8.2%。
- **全部 ≤ 590s、exit 0**（SC-005；最大的 public2 72s，public3 36s）。

## R1 Baseline Fallback 決策

**保留自寫程式碼。** 自寫解在**全部 3 case 皆優於 README Reference**（0.48×–0.61×），且 public1/public3 已**低於 Min**。
依 constitution R1「任一 case 勝 Reference 即保留自寫」，**無需**退回 reference 演算法。
（`reference/obj/*.o` 為 Linux ELF，Windows mingw 無法連結 → 字面 R1「複製 reference 編譯」本就不可行；研究 §10。）

## 方法（與 plan/research 一致，含一處實證修正）

自含解析式佈局（ePlace 家族）：

1. **L1**：Bookshelf 解析（`.aux→.nodes/.nets/.pl/.scl`）→ CSR 資料模型 → 建構式均勻散佈起點（保證合法的 MVP fallback）。
2. **L2**：**LSE 線長代理（含 pin offset、數值平移防溢位）** + **bell-shaped bin 密度**（緊緻支撐、面積守恆 target = 散佈態平均密度）；**Adam** 最佳化，採 **λ 斜坡（penalty method）**：先 WL-主導群聚（依連線把 cell 拉近，HPWL 大降、容許重疊）→ 拉高 λ 把版圖攤回均勻（合法），同時保留排列。
3. **L3**：**OpenMP 平行 FG**（per-net WL、per-cell 密度，**固定順序 reduction → 決定性**）；~560s wall-clock guard + 收斂早停。

**關鍵實證（推翻 research §4「bins 越細越好」）**：**細 bins 反而有害** —— 它們在局部過度均勻化、打散 WL 排列（public3 在 33×33 卡在 1987M；改 20×20 → 358M，提升 5.5×）。
Bins 實為**平滑的全域散佈場**，應**保持粗**（≈14–20/邊、與規模無關，呼應 reference 固定 14×14），才能把 WL 排好的版圖整體「充氣」攤滿 core 而保留相對位置 → HPWL 大幅降低。

**合法性以真實指標把關**：將 scorer 的單列 Tetris legalizer（`legalize.py`）**移植進 C++**，最佳化過程中量測真實 `avgDisp`，並**凍結 λ 於「最密的合法點」**（disp≤0.040），快照最低-HPWL 的合法版圖輸出 —— 直接對齊計分器，而非用 proxy。

## 決定性（FR-013）

固定執行緒數 + 全部 reduction 採**固定順序**（WL/密度 per-thread partial、`schedule(static)`），同輸入重跑**逐位元相同**（public1 重跑座標最大差 = 0）。

## 開發回合數 / 人工介入

- 主 agent 親自完成（R0，無 subagent）。
- 關鍵迭代：optimizer 演進（控制器 → λ 斜坡 penalty method）；密度懲罰由對稱（均勻）定案；**bins 由細改粗**（最大單一提升）；WL 群聚相；真實 legalizer 凍結；`schedule(static)` 修決定性。

## 備註

- 座標慣例：`.pl`/輸出 = 左下角；分析變數 = 中心；pin 全域 = `(cx-w/2+xoff, cy-h/2+yoff)`；HPWL 不加權；core 可為負（已處理）。
- 輸出僅含可動 cell（固定模組由 base `.pl` 補；未移動）。
- 比較原則見 `.specify/memory/constitution.md`。
