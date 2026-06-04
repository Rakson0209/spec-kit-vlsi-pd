# PROMPT.md — 實驗操作手冊（Claude Code / Qwen Code 通用）

照著本檔下指令，就能對 **001 / 002 / 003** 三題、用任一模型跑完整 SDD 流程並計分。
兩個 agent 用的 slash 命令**完全相同**（`/speckit.*`），差別只在「怎麼啟動」與「選哪個模型」。

> 規則依據：`.specify/memory/constitution.md`（v0.3.0）。重點：
> **每模型獨立 spec**（產物進 `experiments/<model>/spec/`）、**研究先行且以超越 baseline 的 Min 為目標**。

---

## 0. 共通概念

- `<model>`：模型資料夾名，自己決定，例如 `claude-sonnet-4-6`、`qwen3-coder-480b`。**同一模型三題共用同一個名字**。
- 每題的 SDD 目錄：`problems/<題目>/experiments/<model>/spec/`
- SDD 七步（兩 agent 皆同，逐條跑、檢視產物再下一步）：

  | 步驟 | 命令 | 產物 |
  |---|---|---|
  | ① 規格 | `/speckit.specify ...` | `spec/spec.md` + checklist |
  | ②（選）澄清 | `/speckit.clarify` | 回填 spec |
  | ③ 計畫＋**研究** | `/speckit.plan` | `spec/research.md`（★以 Min 為門檻調研演算法）/ `plan.md` / `data-model.md` / `contracts/` |
  | ④（選）一致性 | `/speckit.analyze` | 檢查報告 |
  | ⑤ 任務 | `/speckit.tasks` | `spec/tasks.md` |
  | ⑥ 實作 | `/speckit.implement` | `experiments/<model>/` 內原始碼 |

- **研究閘門（原則 VI）**：`/speckit.plan` 的 `research.md` 必須先記下該題 README 的 **Min** 當門檻、調研候選演算法、選定有潛力超越 Min 的方案。未完成不得進入 tasks/implement。

---

## 1. 啟動與選模型

### Claude Code
已在專案根目錄開著本 session 即可。選模型：
```
/model sonnet
```
（`<model>` 用 `claude-sonnet-4-6`）

### Qwen Code
在專案根目錄啟動：
```powershell
qwen
```
模型依 Qwen Code 設定（`.qwen/` 設定或啟動參數）。`<model>` 用對應的 Qwen 模型名。

> 兩邊的 `/speckit.*` 命令相同：Claude 讀 `.claude/commands/`，Qwen 讀 `.qwen/commands/`，內容一致。

---

## 2. 三題的 SDD 命令（把 `<model>` 換成你的模型名）

### 題目 001 — 多技術晶粒切割（Partitioning，指標 cut size）

```
/speckit.specify SPECIFY_FEATURE_DIRECTORY=problems/001-partitioning/experiments/<model>/spec  多技術晶粒切割 (Die Partitioning)。完整題目見 problems/001-partitioning/reference/spec.pdf。輸入 .txt 含 NumTechs/Tech/LibCell(同一 LibCell 在不同 Tech 有不同長寬)、DieSize、DieA/DieB(各自 Tech 與面積使用率上限 utilization%)、NumCells/Cell(指定 LibCell)、NumNets/Net(degree + 連接的 cells)。需求:把每個 cell 指派到 DieA 或 DieB,滿足兩 die 的面積使用率上限與平衡限制,最小化跨 die 的切割 cut size。輸出 .out 為每個 cell 的 die 指派。測資在 benchmark/testcase/(sample + public1~6)。合法性與計分用 scorer/(官方 verifier 為 Linux binary)。目標 cut size ≤ 各 testcase 的 Min(見本題 README 的 Baseline 指標門檻表)。
```
然後依序：`/speckit.clarify`（選）→ `/speckit.plan` → `/speckit.analyze`（選）→ `/speckit.tasks` → `/speckit.implement`

### 題目 002 — 固定輪廓平面規劃（Floorplanning，指標 wirelength）

```
/speckit.specify SPECIFY_FEATURE_DIRECTORY=problems/002-floorplanning/experiments/<model>/spec  固定輪廓平面規劃 (Fixed-outline Floorplanning)。完整題目見 problems/002-floorplanning/reference/spec.pdf。輸入 .txt 含 ChipSize W H、NumSoftModules 與 SoftModule <name> <area>(面積給定、長寬可變形)、NumFixedModules 與 FixedModule <name> <x> <y> <w> <h>(位置尺寸固定)、NumNets 與 Net <A> <B> <weight>。需求:所有模組不重疊且完全落在固定晶片輪廓內,最小化加權線長 HPWL。輸出 .floorplan 為各模組最終座標與尺寸。測資在 benchmark/testcase/(sample + public1~4)。合法性與計分用 scorer/(官方 verifier 為 Linux binary)。目標 wirelength ≤ 各 testcase 的 Min(見本題 README 的 Baseline 指標門檻表)。
```
然後依序：`/speckit.clarify`（選）→ `/speckit.plan` → `/speckit.analyze`（選）→ `/speckit.tasks` → `/speckit.implement`

### 題目 003 — 全域佈局（Global Placement，指標 HPWL）

```
/speckit.specify SPECIFY_FEATURE_DIRECTORY=problems/003-global-placement/experiments/<model>/spec  全域佈局 (Global Placement)。完整題目見 problems/003-global-placement/reference/spec.pdf。輸入為 Bookshelf 格式:一個 .aux 指向 .nodes(cell 與尺寸)/.nets(net 連接)/.pl(初始座標)/.scl(row/site)/.wts(net 權重)。需求:求每個 cell 的座標使半周長線長 HPWL 最小化。輸出 .gp.pl 為各 cell 最終座標(Bookshelf placement 格式)。測資在 benchmark/testcase/public1~3/(每題一個目錄,入口是該目錄下的 .aux)。計分用 scorer/(003 已純 Python,Windows 可跑)。目標 HPWL ≤ 各 testcase 的 Min(見本題 README 的 Baseline 指標門檻表)。
```
然後依序：`/speckit.clarify`（選）→ `/speckit.plan` → `/speckit.analyze`（選）→ `/speckit.tasks` → `/speckit.implement`

---

## 3. 編譯 → 執行 → 計分（PowerShell，Windows 原生）

> 編譯基準 `g++ -std=c++11 -O3`（需先安裝 MSYS2 g++）。
> 以下假設 `/speckit.implement` 把程式放在 `experiments/<model>/`，主程式可編成 `hw.exe`。
> 視模型實際產出的檔案結構微調（原始碼位置、是否有 Makefile）。
> **所有指令一律從專案根目錄執行**（用相對路徑，不含任何絕對路徑）。

### 共同：設定模型名
```powershell
$model = "claude-sonnet-4-6"   # ← 換成你的 <model>
```

### 題目 001
```powershell
$d = "problems\001-partitioning\experiments\$model"
g++ -std=c++11 -O3 -o "$d\hw.exe" (Get-ChildItem -Recurse "$d" -Filter *.cpp | % FullName)
New-Item -ItemType Directory -Force "$d\out" | Out-Null
foreach ($t in 'public1','public2','public3','public4','public5','public6') {
  $ms = Measure-Command { & ".\$d\hw.exe" "problems\001-partitioning\benchmark\testcase\$t.txt" "$d\out\$t.out" }
  Write-Output "$t : $($ms.TotalSeconds)s"
}
python scorer\score.py 001 --output-dir "$d\out" --label $model --md "docs\001-$model.md" --csv "docs\001-$model.csv"
```
> 註：001 baseline 需 Boost；模型實作最好**自寫資料結構、避免相依 Boost**，以利純 Windows 編譯。

### 題目 002
```powershell
$d = "problems\002-floorplanning\experiments\$model"
g++ -std=c++11 -O3 -o "$d\hw.exe" (Get-ChildItem -Recurse "$d" -Filter *.cpp | % FullName)
New-Item -ItemType Directory -Force "$d\out" | Out-Null
foreach ($t in 'public1','public2','public3','public4') {
  $ms = Measure-Command { & ".\$d\hw.exe" "problems\002-floorplanning\benchmark\testcase\$t.txt" "$d\out\$t.floorplan" }
  Write-Output "$t : $($ms.TotalSeconds)s"
}
python scorer\score.py 002 --output-dir "$d\out" --label $model --md "docs\002-$model.md" --csv "docs\002-$model.csv"
```

### 題目 003
```powershell
$d = "problems\003-global-placement\experiments\$model"
g++ -std=c++11 -O3 -o "$d\hw.exe" (Get-ChildItem -Recurse "$d" -Filter *.cpp | % FullName)
New-Item -ItemType Directory -Force "$d\out" | Out-Null
foreach ($t in 'public1','public2','public3') {
  $ms = Measure-Command { & ".\$d\hw.exe" "problems\003-global-placement\benchmark\testcase\$t\$t.aux" "$d\out\$t.gp.pl" }
  Write-Output "$t : $($ms.TotalSeconds)s"
}
python scorer\score.py 003 --output-dir "$d\out" --label $model --md "docs\003-$model.md" --csv "docs\003-$model.csv"
```

> **更快的替代**：scorer 的 `--run "$d\hw.exe"` 會自動對每個 testcase 執行你的 exe 再計分（一行搞定，但不量 runtime）。
> 想記錄 runtime（要 <上限）就用上面的 `Measure-Command` 手動跑法。
> **輸出檔名規則**：scorer 找 `<case><副檔名>`（001=`.out`、002=`.floorplan`、003=`.gp.pl`），別改名。

---

## 4. 記錄與比較（原則 IV / VI）

每題在 `experiments/<model>/RESULT.md` 記錄：
- 模型 / 版本、編譯指令、執行指令
- 各 testcase 的指標數值 + runtime
- **是否 ≤ Min**（超越打勾；未超越要寫差距與原因分析）
- 開發回合數 / 人工介入次數

各題的 Min 門檻見該題 README 的「Baseline 指標門檻」表：
[001](problems/001-partitioning/README.md) · [002](problems/002-floorplanning/README.md) · [003](problems/003-global-placement/README.md)

### Min 門檻速查（要 ≤ 此值）

| | public1 | public2 | public3 | public4 | public5 | public6 | 上限 |
|---|---:|---:|---:|---:|---:|---:|---:|
| **001 cut size** | 104 | 816 | 1,762 | 982 | 297 | 5,159 | ~300s |
| **002 wirelength** | 161,609,972 | 20,966,863 | 1,856,276 | 63,024,850 | — | — | ~600s |
| **003 HPWL** | 59,788,412 | 10,530,075 | 395,131,978 | — | — | — | ~590s |

---

## 5. 換模型 / 換 agent

- **換模型**：所有命令把 `<model>` 換成新名字即可，產物自然進不同資料夾，互不干擾。
- **一次一條鏈**：`.specify/feature.json` 一次只記一個 active 目錄，請**一個模型一題跑完**再換下一個。
- **官方 verifier**：純 Windows 無法跑（Linux ELF）。要最終正確性背書可在 **WSL/Linux** 跑 `benchmark/verifier/verify`，或 scorer 加 `--verify`（需 Linux）。
