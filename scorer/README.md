# scorer — VLSI PD 批次計分器

官方 `verifier/verify` 只回報合法性（`[Success]/[Error]`），**不給最佳化指標數值**。
本工具補上這塊：**重新計算每題的最佳化指標**、做合法性檢查、（選用）呼叫官方 verify，
並把跨 testcase / 跨模型的結果**彙整成表格**，供 `docs/` 的比較使用。

## 需求

- Python 3.7+（純標準函式庫，無第三方依賴）。
- 003 placement 需先編譯官方 HPWL 包裝（見下），且為 Linux 專用。

## 三題指標與合法性

| 題目 | 指標（越小越好） | 計算方式 | 合法性檢查 |
|---|---|---|---|
| 001 partitioning | **cut size** | 同時有 cell 落在 DieA、DieB 的 net 數 | 各晶粒面積使用率 `area/(W·H) ≤ util`（各用自身 Tech 尺寸）、cell 恰分配一次 |
| 002 floorplanning | **weighted HPWL** | `Σ weight·(\|cx1-cx2\|+\|cy1-cy2\|)`，pin 在模組中心、中心向下取整 | 輪廓內、面積≥min、長寬比∈[0.5,2]、無重疊、硬模組一致 |
| 003 global placement | **HPWL** | 由題目官方預編譯庫 `computeHpwl()` 計算（與官方 hw4 一致） | 交給官方 `verifier/verify` |

> 001/002 為純 Python 計算（跨平台）；003 透過 C++ helper 呼叫官方函式庫，確保與評分機制完全一致。

## 003：先編譯官方 HPWL 包裝（Linux）

```sh
cd problems/003-global-placement/scorer
make            # 連結 ../reference/obj/*.o，產生 hpwl_eval
```

`hpwl_eval <.aux> <.gp.pl>` 會載入「題目 .aux + 模型輸出 .gp.pl」並印出官方 HPWL。
計分器會自動找到並呼叫它。

## 用法

```sh
# 1) 已有輸出，直接計分（partitioning/floorplanning 可在任何平台跑）
python scorer/score.py 001 --output-dir <模型輸出目錄> --label <模型名> \
       --md docs/001-<模型>.md --csv docs/001-<模型>.csv

# 2) 用實作執行檔自動對每個 testcase 產生輸出再計分（Linux：baseline/實作多為 ELF）
python scorer/score.py 002 --run <實作執行檔> --output-dir /tmp/out --label reference

# 3) 同時呼叫官方 verifier 判定合法性（Linux）
python scorer/score.py 003 --run <hw4> --output-dir /tmp/gp --label reference --verify

# 只跑部分 testcase
python scorer/score.py 001 --output-dir <dir> --cases sample,public1
```

### 參數

| 參數 | 說明 |
|---|---|
| `problem` | `001`/`002`/`003` 或別名（partitioning/floorplanning/placement） |
| `--output-dir` | 模型輸出檔所在目錄；檔名須為 `<case><副檔名>`（`.out`/`.floorplan`/`.gp.pl`） |
| `--testcase-dir` | testcase 根目錄（預設該題 `benchmark/testcase`） |
| `--run EXE` | 對每個 testcase 執行 `EXE <input> <output>` 先產生輸出 |
| `--verify` | 呼叫該題 `benchmark/verifier/verify`（Linux ELF） |
| `--cases a,b` | 只評測指定 case |
| `--label` | 此次評測標籤（模型名） |
| `--md` / `--csv` | 輸出 Markdown / CSV 報表 |

### 輸出

- stdout：逐 case 進度 + 彙整表格 + 摘要。
- `--md`：Markdown 表格（可貼進 `docs/README.md` 的比較表）。
- `--csv`：欄位 `label, problem, case, metric_name, metric, valid, verifier, self_reported, violations`，
  方便跨模型合併分析。

> `self_reported` 是輸出檔自報的指標（如 `CutSize`/`Wirelength`），`metric` 是計分器**獨立重算**的值；
> 兩者不一致代表該實作自報造假或計算有誤。比較一律以重算的 `metric` 為準。

## 平台註記

`baseline`、官方 `verify`、`hpwl_eval`（連結 Linux `.o`）皆為 **Linux** 執行檔；
請在 Linux/WSL 進行「執行實作 / 跑 verify / 算 003 HPWL」。
001/002 的純計分與彙整可在 Windows 直接執行。
