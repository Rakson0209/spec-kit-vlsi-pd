# PROMPT.md — 實驗操作手冊（純 Windows）

比較不同模型的 `/speckit.implement` 實作品質。
規格（spec/plan/tasks）由 `claude-opus-4-8` 統一產出；各受測模型各自執行 implement。

> 規則依據：`.specify/memory/constitution.md`

---

## 0. 共通概念

| 術語 | 說明 |
|---|---|
| `<model>` | 受測模型資料夾名，例如 `claude-sonnet-4-6`、`deepseek-r1` |
| Phase 1 | 規劃：由 claude-opus-4-8 執行，每題**一次** |
| Phase 2 | 實作：各模型各自執行，產物進各自目錄 |
| 共用 spec 目錄 | `problems/<NNN-題目>/experiments/claude-opus-4-8/spec/` |

---

## 1. Phase 1：規劃（claude-opus-4-8 執行，每題一次）

> 用 `/model opus` 切換至 claude-opus-4-8。

### 題目 001 — 多技術晶粒切割

```
/speckit.specify SPECIFY_FEATURE_DIRECTORY=problems/001-partitioning/experiments/claude-opus-4-8/spec  多技術晶粒切割 (Die Partitioning)。完整題目見 problems/001-partitioning/reference/spec.pdf。輸入 .txt 含 NumTechs/Tech/LibCell(同一 LibCell 在不同 Tech 有不同長寬)、DieSize、DieA/DieB(各自 Tech 與面積使用率上限 utilization%)、NumCells/Cell(指定 LibCell)、NumNets/Net(degree + 連接的 cells)。需求:把每個 cell 指派到 DieA 或 DieB,滿足兩 die 的面積使用率上限與平衡限制,最小化跨 die 的切割 cut size。輸出 .out 為每個 cell 的 die 指派。測資在 benchmark/testcase/(sample + public1~6)。合法性與計分一律用 scorer/(純 Python,Windows 可跑)。驗收:先確保全部 testcase 經 scorer 合法,再追求 cut size ≤ 各 testcase 的 Min(見本題 README 的 Baseline 指標門檻表)。
```

依序：`/speckit.plan` → `/speckit.tasks`

### 題目 002 — 固定輪廓平面規劃

```
/speckit.specify SPECIFY_FEATURE_DIRECTORY=problems/002-floorplanning/experiments/claude-opus-4-8/spec  固定輪廓平面規劃 (Fixed-outline Floorplanning)。完整題目見 problems/002-floorplanning/reference/spec.pdf。輸入 .txt 含 ChipSize W H、NumSoftModules 與 SoftModule(面積給定、長寬可變形)、NumFixedModules 與 FixedModule(位置尺寸固定)、NumNets 與 Net(A B weight)。需求:所有模組不重疊且完全落在固定晶片輪廓內,最小化加權線長 HPWL。輸出 .floorplan 為各模組最終座標與尺寸。測資在 benchmark/testcase/(sample + public1~4)。合法性與計分一律用 scorer/(純 Python,Windows 可跑)。驗收:先確保全部 testcase 經 scorer 合法(不重疊、在輪廓內、面積/長寬比符合),再追求 wirelength ≤ 各 testcase 的 Min(見本題 README 的 Baseline 指標門檻表)。
```

依序：`/speckit.plan` → `/speckit.tasks`

### 題目 003 — 全域佈局

```
/speckit.specify SPECIFY_FEATURE_DIRECTORY=problems/003-global-placement/experiments/claude-opus-4-8/spec  全域佈局 (Global Placement)。完整題目見 problems/003-global-placement/reference/spec.pdf。輸入為 Bookshelf 格式:一個 .aux 指向 .nodes(cell 與尺寸)/.nets(net 連接)/.pl(初始座標)/.scl(row/site)/.wts(net 權重)。需求:求每個 cell 的座標使半周長線長 HPWL 最小化。輸出 .gp.pl 為各 cell 最終座標。測資在 benchmark/testcase/public1~3/。合法性與計分一律用 scorer/(純 Python,Windows 可跑)。驗收:先確保全部 testcase 經 scorer 合法(模組在 core 內、固定模組未移動),再追求 HPWL ≤ 各 testcase 的 Min(見本題 README 的 Baseline 指標門檻表)。
```

依序：`/speckit.plan` → `/speckit.tasks`

---

## 2. Phase 2：實作（各受測模型執行）

> 換模型後，先複製 opus 的 spec 資料夾到該模型目錄，再執行 implement。

### Step 1：複製 spec（每題每模型執行一次）

```powershell
$problem = "001-partitioning"   # 換成目標題目
$model   = "claude-sonnet-4-6"  # 換成受測模型名
Copy-Item -Recurse "problems\$problem\experiments\claude-opus-4-8\spec" `
          "problems\$problem\experiments\$model\spec"
```

> 複製後各模型擁有自己的 `spec/` 副本，implement 過程中的筆記、任務狀態更新不會影響 opus 的原始規格。

### Step 2：執行 implement（指向自己的 spec 副本）

```
/speckit.implement SPECIFY_FEATURE_DIRECTORY=problems/<NNN-題目>/experiments/<model>/spec  請依 tasks.md 實作，程式碼放在 problems/<NNN-題目>/experiments/<model>/main.cpp。依 R1 Baseline Fallback：先自寫 code → 全 testcase 跑 scorer → 若全部 case 都比 reference/src/ 差才複製 reference 並優化；任一 case 優於 reference 就繼續用自己的 code。全 testcase 合法（scorer OK）才算完成。
```

---

## 3. 編譯 → 執行 → 計分

> **每次開新 PowerShell 先載入環境**：
> ```powershell
> . .\tools\mingw64\setup-env.ps1
> ```

> **★ 執行限制**：本機安全政策封鎖一般路徑下自編 exe，**唯獨 `D:\FSecret\` 允許執行**。exe 一律編到 `D:\FSecret\` 再執行；輸出資料檔不受限。

### 共同：設定變數

```powershell
$model  = "claude-sonnet-4-6"   # ← 換成受測模型名
$exeDir = "D:\FSecret"
```

### 題目 001

```powershell
$d   = "problems\001-partitioning\experiments\$model"
$exe = "$exeDir\hw_001_$model.exe"
g++ -std=c++20 -O3 -fopenmp -pthread -o $exe (Get-ChildItem -Recurse "$d" -Filter *.cpp | % FullName)
New-Item -ItemType Directory -Force "$d\out" | Out-Null
foreach ($t in 'sample','public1','public2','public3','public4','public5','public6') {
  $ms = Measure-Command { & $exe "problems\001-partitioning\benchmark\testcase\$t.txt" "$d\out\$t.out" }
  Write-Output "$t : $($ms.TotalSeconds)s"
}
python scorer\score.py 001 --output-dir "$d\out" --label $model --md "docs\001-$model.md" --csv "docs\001-$model.csv"
```

### 題目 002

```powershell
$d   = "problems\002-floorplanning\experiments\$model"
$exe = "$exeDir\hw_002_$model.exe"
g++ -std=c++20 -O3 -fopenmp -pthread -o $exe (Get-ChildItem -Recurse "$d" -Filter *.cpp | % FullName)
New-Item -ItemType Directory -Force "$d\out" | Out-Null
foreach ($t in 'sample','public1','public2','public3','public4') {
  $ms = Measure-Command { & $exe "problems\002-floorplanning\benchmark\testcase\$t.txt" "$d\out\$t.floorplan" }
  Write-Output "$t : $($ms.TotalSeconds)s"
}
python scorer\score.py 002 --output-dir "$d\out" --label $model --md "docs\002-$model.md" --csv "docs\002-$model.csv"
```

### 題目 003

```powershell
$d   = "problems\003-global-placement\experiments\$model"
$exe = "$exeDir\hw_003_$model.exe"
g++ -std=c++20 -O3 -fopenmp -pthread -o $exe (Get-ChildItem -Recurse "$d" -Filter *.cpp | % FullName)
New-Item -ItemType Directory -Force "$d\out" | Out-Null
foreach ($t in 'public1','public2','public3') {
  $ms = Measure-Command { & $exe "problems\003-global-placement\benchmark\testcase\$t\$t.aux" "$d\out\$t.gp.pl" }
  Write-Output "$t : $($ms.TotalSeconds)s"
}
python scorer\score.py 003 --output-dir "$d\out" --label $model --md "docs\003-$model.md" --csv "docs\003-$model.csv"
```

---

## 4. 合法性閘門（必須通過才算完成）

scorer 表格的「合法」欄每個 testcase 應為 **OK**。
**任一 NG = 未完成**：回去修正 → 重編 → 重跑 → 重計分，直到全部 OK，才看指標是否 ≤ Min。

---

## 5. Min 門檻速查（要 ≤ 此值；前提是該 case 已合法）

| | public1 | public2 | public3 | public4 | public5 | public6 | 上限 |
|---|---:|---:|---:|---:|---:|---:|---:|
| **001 cut size** | 104 | 816 | 1,762 | 982 | 297 | 5,159 | ~300s |
| **002 wirelength** | 161,609,972 | 20,966,863 | 1,856,276 | 63,024,850 | — | — | ~600s |
| **003 HPWL** | 59,788,412 | 10,530,075 | 395,131,978 | — | — | — | ~590s |

---

## 6. 結果記錄

每題在 `experiments/<model>/RESULT.md` 記錄：
- 模型名稱 / 編譯指令
- 各 testcase：合法 OK/NG + 指標數值 + runtime
- 是否全部合法；合法後是否 ≤ Min（未超越要寫差距）

各題 Min 門檻：[001](problems/001-partitioning/README.md) · [002](problems/002-floorplanning/README.md) · [003](problems/003-global-placement/README.md)
