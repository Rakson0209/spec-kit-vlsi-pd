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

- **003-global-placement** (shared spec, Phase 1 by claude-opus-4-8) — ✅ **opus implement done**
  ([main.cpp](problems/003-global-placement/experiments/claude-opus-4-8/main.cpp),
  [RESULT.md](problems/003-global-placement/experiments/claude-opus-4-8/RESULT.md)): **3/3 legal**,
  **beats Reference on all 3** (0.48×–0.61×), **below Min on public1 & public3** (public1 46.6M vs Min 59.8M ·
  public2 11.39M vs Ref 18.6M, +8% Min · public3 358M vs Min 395M); avgDisp 0.035–0.041× (<0.05), ≤72s,
  **deterministic** (bit-exact reruns). **R1 = keep self-written.** Spec:
  [plan.md](problems/003-global-placement/experiments/claude-opus-4-8/spec/plan.md) ·
  [research.md](problems/003-global-placement/experiments/claude-opus-4-8/spec/research.md) ·
  [data-model.md](problems/003-global-placement/experiments/claude-opus-4-8/spec/data-model.md) ·
  [contracts/](problems/003-global-placement/experiments/claude-opus-4-8/spec/contracts/) ·
  [quickstart.md](problems/003-global-placement/experiments/claude-opus-4-8/spec/quickstart.md)
  — **Approach**: self-contained analytical placement (ePlace family). L1 Bookshelf parse + CSR + constructive
  legal spread → L2 **LSE wirelength (pin-offset, shifted) + bell bin-density + λ-ramp penalty via Adam**:
  WL-dominant **cluster phase** (cells pile by connectivity, overlap OK) → ramp λ to spread back to legal
  preserving arrangement → L3 **OpenMP FG** (fixed-order reduction = determinism) + ~560s guard.
  **Key empirical finding (overturns research §4)**: *finer* bins **hurt** — they scramble the WL arrangement
  (public3 33×33 → 1987M; **20×20 → 358M**, 5.5×); bins are a **global** spreading field — keep them **coarse**
  (≈14–20/side, like reference's fixed 14×14). Legality gated by **porting scorer's Tetris legalizer into C++**
  and freezing λ at the densest legal point (real avgDisp, not a proxy). Metric = **unweighted** pin-offset
  HPWL; coords lower-left, clamp-in-core. **R1 caveat**: `reference/obj/*.o` are **Linux ELF → unlinkable on
  Windows**, code is self-contained. Min/Reference/Max in
  [spec.md](problems/003-global-placement/experiments/claude-opus-4-8/spec/spec.md).

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

