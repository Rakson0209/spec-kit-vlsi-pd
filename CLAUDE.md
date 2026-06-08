# CLAUDE.md — spec-kit-vlsi-pd

## 專案說明

以 SDD 流程實作 VLSI 實體設計題目，**比較不同模型在相同規格下的實作（coding）能力**。

- **規劃（specify → plan → tasks）**：由 `claude-opus-4-8` 統一負責，產出各題共用 spec。
- **實作（implement）**：各受測模型各自執行 `/speckit.implement`，比較產出品質。

Constitution: `.specify/memory/constitution.md`

## 核心規則（最重要）

**R0 禁止委派**：禁止使用 subagent（`runSubagent`）委派任務。所有工作必須由主 agent 親自完成。

**R1 Baseline Fallback**：先自寫 code → 跑 scorer → 若**全部** case 都比 `reference/src/` 差 → 複製 reference 並在其上優化。只要有任何 case 優於 reference，就繼續用自己的 code。

**R2 平行優先**：編譯一律 `g++ -std=c++20 -O3 -fopenmp -pthread`。優先使用 OpenMP 平行化。

**R3 合法性 + 全 case**：所有 testcase scorer OK。每次改 code 後對所有 case 跑 scorer。

## 實驗架構

```
problems/NNN/experiments/
├── claude-opus-4-8/
│   ├── spec/          ← Phase 1 產物（所有模型的 spec 來源）
│   │   ├── spec.md
│   │   ├── plan.md
│   │   ├── tasks.md
│   │   └── research.md
│   └── main.cpp       ← opus 自身實作
├── claude-sonnet-4-6/
│   ├── spec/          ← 從 opus spec/ 複製而來（Phase 2 起點）
│   └── main.cpp
└── <其他模型>/
    ├── spec/          ← 從 opus spec/ 複製而來
    └── main.cpp
```

## 工作流程

### Phase 1：規劃（以 claude-opus-4-8 執行，每題一次）

```
/speckit.specify → /speckit.plan → /speckit.tasks
```

產物目錄：`problems/NNN/experiments/claude-opus-4-8/spec/`

### Phase 2：實作比較（各受測模型各自執行）

**Step 1：複製 opus spec 到該模型目錄**

```powershell
$problem = "001-partitioning"   # 換題目
$model   = "claude-sonnet-4-6"  # 換模型
Copy-Item -Recurse "problems\$problem\experiments\claude-opus-4-8\spec" `
          "problems\$problem\experiments\$model\spec"
```

**Step 2：執行 implement（指向該模型自己的 spec 副本）**

```
/speckit.implement SPECIFY_FEATURE_DIRECTORY=problems/<NNN>/experiments/<model>/spec
```

各模型在自己的 `spec/` 副本下實作，產物與筆記不會影響 opus 原始規格。

## 編譯

```powershell
. .\tools\mingw64\setup-env.ps1
g++ -std=c++20 -O3 -fopenmp -pthread -o <exe> main.cpp
```

## 計分

```powershell
python scorer/score.py <problem-num> --output-dir <out-dir> --label <model>
```

## SDD Skill 檔案

`.claude/skills/speckit-*/SKILL.md`

<!-- SPECKIT START -->
## Active Plan (managed by speckit)

- **003-global-placement** (shared spec, Phase 1 by claude-opus-4-8) — ✅ **plan ready**:
  [plan.md](problems/003-global-placement/experiments/claude-opus-4-8/spec/plan.md) ·
  [research.md](problems/003-global-placement/experiments/claude-opus-4-8/spec/research.md) ·
  [data-model.md](problems/003-global-placement/experiments/claude-opus-4-8/spec/data-model.md) ·
  [contracts/](problems/003-global-placement/experiments/claude-opus-4-8/spec/contracts/) ·
  [quickstart.md](problems/003-global-placement/experiments/claude-opus-4-8/spec/quickstart.md)
  — **Approach**: flat **analytical global placement** of std cells, **HPWL min subject to spreading**.
  Key trap: HPWL scored on *overlapping* placement, but scorer's row-Tetris **anti-collapse health check**
  rejects piles (`avgDisp ≤ 0.05×min(coreW,coreH)`) — so the **density term is what makes it legal**, not
  polish ([legalize.py](scorer/lib/legalize.py)). L1 parser + CSR data model + constructive legal spread →
  L2 **WA/LSE wirelength + bell-shaped bin-density + λ-ramp CG** (own solver), WL-aware init → L3
  **OpenMP-parallel FG** + **adaptive bins** (reference's fixed 14×14 too coarse) + ~560s wall-clock guard +
  multi-start. Metric = **unweighted** pin-offset HPWL (`.wts` ignored by scorer). Coords lower-left,
  clamp-in-core on output. **R1 caveat**: `reference/obj/*.o` are **Linux ELF → unlinkable on Windows**, so
  "copy reference/src" is infeasible; code is **self-contained**, R1 = *port reference algorithm not binary*.
  Min/Reference/Max in [spec.md](problems/003-global-placement/experiments/claude-opus-4-8/spec/spec.md)
  (public1 Min 59.8M / Ref 88.0M · public2 10.5M / 18.6M · public3 395.1M / 750.9M, ≤590s).

- **002-floorplanning** (shared spec, Phase 1 by claude-opus-4-8) — ✅ **opus implement done**
  ([main.cpp](problems/002-floorplanning/experiments/claude-opus-4-8/main.cpp),
  [RESULT.md](problems/002-floorplanning/experiments/claude-opus-4-8/RESULT.md)): 5/5 legal, beats
  Reference on all 4 public (0.45×–0.73×), **beats Min on public1 0.978× & public4 0.990×** (public3
  1.034×, public2 1.151×), ≤322s, R1 = keep self-written. Spec:
  [plan.md](problems/002-floorplanning/experiments/claude-opus-4-8/spec/plan.md) ·
  [research.md](problems/002-floorplanning/experiments/claude-opus-4-8/spec/research.md) ·
  [data-model.md](problems/002-floorplanning/experiments/claude-opus-4-8/spec/data-model.md) ·
  [contracts/](problems/002-floorplanning/experiments/claude-opus-4-8/spec/contracts/) ·
  [quickstart.md](problems/002-floorplanning/experiments/claude-opus-4-8/spec/quickstart.md)
  — **Approach**: fixed-outline floorplanning, **pure-wirelength** objective (drop reference's `α·area`).
  L1 guaranteed-legal shape gen + **grid-free** bottom-left pack (no `H×W` grid; `O(n²)` rect math) →
  L2 **weighted-median** coordinate descent + SA (translate/swap/reshape, incremental HPWL) →
  L3 **OpenMP parallel multi-start**. Pins at integer-floor centers `(x+w//2, y+h//2)`; strict-inequality
  overlap (edge-touch legal), all integers (R6). R1 fallback = copy `reference/src` (**Boost-free** — no
  porting) + pure-WL + parallel. Min/Reference/Max targets in
  [spec.md](problems/002-floorplanning/experiments/claude-opus-4-8/spec/spec.md).

- **001-partitioning** (shared spec, Phase 1 by claude-opus-4-8) — plan ready:
  [plan.md](problems/001-partitioning/experiments/claude-opus-4-8/spec/plan.md) — layered hypergraph
  min-cut (feasible greedy init → area-constrained **FM** → **parallel multi-start** + multilevel).
<!-- SPECKIT END -->

