# scorer — VLSI PD 批次計分器（純 Python / Windows 原生）

官方 `verifier/verify` 只回報合法性（`[Success]/[Error]`），**不給最佳化指標數值**。
本工具補上這塊：**重新計算每題最佳化指標**、做合法性檢查、把跨 testcase / 跨模型結果**彙整成表格**。

> **純 Python、零外部依賴、Windows 原生可跑**——不需編譯、不需 Linux、不需 WSL。
> 三題（含 003 HPWL）的計分都用 Python 完成，已在純 Windows 環境實測。

## 需求

- Python 3.7+（只用標準函式庫）。就這樣。

## 三題指標與合法性

| 題目 | 指標（越小越好） | 計算方式 | 合法性檢查 |
|---|---|---|---|
| 001 partitioning | **cut size** | 同時有 cell 落在 DieA、DieB 的 net 數 | 各晶粒面積使用率 `area/(W·H) ≤ util`（各用自身 Tech 尺寸）、cell 恰分配一次 |
| 002 floorplanning | **weighted HPWL** | `Σ weight·(\|cx1-cx2\|+\|cy1-cy2\|)`，pin 在模組中心、中心向下取整 | 輪廓內、面積≥min、長寬比∈[0.5,2]、無重疊、硬模組一致 |
| 003 global placement | **HPWL**（重疊態 global） | 純 Python 解析 bookshelf，pin 全域座標 = 模組左下 + pin offset，`Σ(maxx-minx)+(maxy-miny)` | 可動模組皆有座標且在 core 內、固定模組(terminal/FIXED)未被移動、**可合法化健康度**（內建 row-based legalizer 攤平後平均位移 ≤ 0.05×min(coreW,coreH)，封住「塌縮成一點得近零 HPWL」騙分） |

### 數值正確性（已驗證）

- 001：sample 手算 cut size=2 ✓；面積超標案例正確判 NG ✓
- 002：sample 手算 weighted HPWL=215 ✓
- 003：迷你手算案例 HPWL=210 ✓；public1（12028 模組 / 11507 net）0.3 秒解析完成 ✓

## 用法

```sh
# 已有輸出，直接計分（三題都可在 Windows 直接跑）
python scorer/score.py 001 --output-dir <模型輸出目錄> --label <模型名> \
       --md docs/001-<模型>.md --csv docs/001-<模型>.csv

# 只跑部分 testcase
python scorer/score.py 003 --output-dir <dir> --cases public1,public2

# 若有可執行的實作，讓 scorer 先跑出輸出再計分
python scorer/score.py 002 --run <實作執行檔> --output-dir <輸出目錄> --label reference
```

### 參數

| 參數 | 說明 |
|---|---|
| `problem` | `001`/`002`/`003` 或別名（partitioning/floorplanning/placement） |
| `--output-dir` | 模型輸出檔所在目錄；檔名須為 `<case><副檔名>`（`.out`/`.floorplan`/`.gp.pl`） |
| `--testcase-dir` | testcase 根目錄（預設該題 `benchmark/testcase`） |
| `--run EXE` | 對每個 testcase 執行 `EXE <input> <output>` 先產生輸出（需該執行檔可在本機跑） |
| `--cases a,b` | 只評測指定 case |
| `--label` | 此次評測標籤（模型名） |
| `--md` / `--csv` | 輸出 Markdown / CSV 報表 |
| `--verify` | （選用，需 Linux）呼叫官方 `verify`；純 Windows 不可用，見下 |

### 輸出

- stdout：逐 case 進度 + 彙整表格 + 摘要。
- `--md`：Markdown 表格（可貼進 `docs/README.md` 的比較表）。
- `--csv`：欄位 `label, problem, case, metric_name, metric, valid, verifier, self_reported, violations`。

> `self_reported` 是輸出檔自報的指標（如 `CutSize`/`Wirelength`），`metric` 是計分器**獨立重算**的值；
> 兩者不一致代表該實作自報造假或計算有誤。比較一律以重算的 `metric` 為準。

## 關於合法性與「官方對標」

你的環境是**純 Windows**，官方 `verify`、`baseline`、003 的 `obj/*.o` 都是 **Linux ELF**，無法執行。
因此本工具的合法性以**內建 Python 檢查**為準（已涵蓋各題主要規則），`--verify` 僅在 Linux 有效。

對「**比較不同模型**」這個目的，重點是用**同一把尺**量所有模型——
本工具對所有模型套用一致的指標定義，相對高下即有效，不依賴官方絕對值。

若日後需要與課程官方分數**逐位元對標**，003 另附一支 Linux 專用工具
`problems/003-global-placement/scorer/hpwl_eval.cpp`（連結官方 `obj` 呼叫 `computeHpwl()`），
在 Linux `make` 後即可比對。純 Windows 用不到它，可忽略。
