---
description: "Task list for Global Placement (HPWL minimization) implementation"
---

# Tasks: Global Placement (HPWL Minimization)

**Input**: Design documents in `problems/003-global-placement/experiments/claude-opus-4-8/spec/`
**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md), [data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md)

## Conventions (read first)

- **`IMPL/` = `problems/003-global-placement/experiments/<model>/`** — the implementing model's directory. **All code paths below are relative to `IMPL/`.** For opus's own implementation, `<model>` = `claude-opus-4-8`. (This `tasks.md` is shared: it is copied into each model's `spec/` and each model implements its own `IMPL/main.cpp`.)
- **Single translation unit**: the whole solver lives in `IMPL/main.cpp` (plan.md). Therefore **most tasks edit the same file and are sequential** — `[P]` is used only for genuinely separate files. The real parallelism is at **runtime** (OpenMP-parallel objective/gradient + multi-start, US3), not in this task graph.
- **Self-contained, NO `reference/obj`**: `reference/obj/*.o` are **Linux ELF** and cannot be linked by the Windows mingw toolchain (research §10). The tool implements its own Bookshelf parser, data model, objective, and optimizer — zero dependency on the reference binary.
- **Testing = the scorer** (constitution R6). No separate unit-test code is generated; each story's "validation" task runs `scorer/score.py` (the project's acceptance test). Tests were **not** requested in the spec.
- **The anti-collapse trap** (research §2, [legalize.py](../../../../../scorer/lib/legalize.py)): HPWL is scored on the *overlapping* placement, but the scorer Tetris-spreads the output and rejects it if `avg disp > 0.05 × min(coreW,coreH)`. **The density term is what makes a low-HPWL solution legal.** Every validation task MUST check the `avgDisp` note, not just `valid`.
- **Build/run/score commands**: see [quickstart.md](./quickstart.md). Build flags are fixed: `g++ -std=c++20 -O3 -fopenmp -pthread` (**no Boost**, no `reference/obj`). **Exec constraint**: the compiled `hw4.exe` must live under **`D:\FSecret\`** to run (Defender ASR); output files may live anywhere.
- **Coordinate conventions** (research §7, [io-format.md](./contracts/io-format.md)): `.pl`/output `(x,y)` = **lower-left**; analytical variable = **center**; `center = lowerleft + (w/2,h/2)`. Pin global = `(cell.x+xoff, cell.y+yoff)`. HPWL is **unweighted** (`.wts` ignored by the scorer). Core bounds may be **negative**.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: different file, no dependency on an incomplete task → may run in parallel
- **[Story]**: `[US1]`/`[US2]`/`[US3]` for user-story phases (Setup/Foundational/Polish carry no story label)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: scaffold the experiment directory and confirm the toolchain builds.

- [ ] T001 Create `IMPL/main.cpp` skeleton (`int main(int argc,char**argv)`) and the `IMPL/out/` output directory
- [ ] T002 [P] Create `IMPL/Makefile` with target `g++ -std=c++20 -O3 -fopenmp -pthread -o hw4 main.cpp` (**no** `-I boost`, **no** `../obj/*.o` link — research §10)
- [ ] T003 Load toolchain (`. .\tools\mingw64\setup-env.ps1`, or reload PATH per [quickstart.md](./quickstart.md)), build the skeleton to `D:\FSecret\hw4.exe`, and confirm it runs and accepts two path args (`<input.aux> <output.gp.pl>`, per [contracts/cli-contract.md](./contracts/cli-contract.md))

**Checkpoint**: empty binary builds with the project flags and runs from `D:\FSecret\`.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: the Bookshelf I/O + data + geometry layer every story depends on. **No user story can begin until this is done.** All tasks edit `IMPL/main.cpp` → sequential.

- [ ] T004 Implement CLI handling + deadline in `IMPL/main.cpp`: read `argv[1]` (`.aux`) / `argv[2]` (`.gp.pl`), create the output's parent dir if missing, capture a wall-clock start (`omp_get_wtime()`) and set an internal deadline `~560s` (research §9); exit 0 on success, non-zero on wrong arg count (per [contracts/cli-contract.md](./contracts/cli-contract.md))
- [ ] T005 Implement the `.aux` parser + shared tokenizer in `IMPL/main.cpp` per [contracts/io-format.md](./contracts/io-format.md): a `getline`+whitespace tokenizer that **skips blank lines, `#` comments, and the `UCLA` header** (matches `bookshelf._data_lines`); parse the `.aux` line and resolve `.nodes/.nets/.pl/.scl/.wts` paths **relative to the `.aux` directory** by extension
- [ ] T006 Parse `.nodes` + `.pl` in `IMPL/main.cpp` (data-model.md Cell): `.nodes` lines `<name> <w> <h> [terminal]` (skip `NumNodes`/`NumTerminals`); **intern names → contiguous int ids**; `.pl` lines `<name> <x> <y> : <orient> [/FIXED]` (lower-left). Set `fixed = terminal-in-nodes OR /FIXED-in-pl`; store fixed cells' centers as constants; build the **movable index list** `mov[]`
- [ ] T007 Parse `.nets` into **CSR** in `IMPL/main.cpp` (data-model.md Net): `NetDegree : k` then `k` pin lines `<node> <I/O> : <xoff> <yoff>`; fill `net_ptr[]`, `pin_cell[]`, `pin_xoff[]`, `pin_yoff[]` (offsets relative to lower-left); skip `NumNets`/`NumPins`; tolerate degree ≤ 1 / repeated cells without crashing. (Optionally read `.wts`, but **do not** apply weights to the objective — research §3.)
- [ ] T008 Parse `.scl` rows + derive core in `IMPL/main.cpp` (data-model.md Core/Rows, research §7): per `CoreRow` capture `Coordinate`(y), `Height`, `Sitespacing`/`Sitewidth`, `SubrowOrigin`(x0), `NumSites` → `rows[]={y,h,x0,x1,sp}`; core `= (min x0, min y, max(x0+NumSites·sp), max(y+h))`; record `rowHeight`. **Handle negative coordinates.**
- [ ] T009 Implement the scorer-exact HPWL in `IMPL/main.cpp` (research §3/§7, [io-format.md](./contracts/io-format.md)): `totalHPWL = Σ_nets (max−min)pin_x + (max−min)pin_y`, **unweighted**, pin global `= (cell.x+xoff, cell.y+yoff)` with `cell.x = center−w/2`. This is the value any logging prints and the value we compare to Min/Reference/Max.
- [ ] T010 Implement the `.gp.pl` writer in `IMPL/main.cpp` per [contracts/io-format.md](./contracts/io-format.md): a `UCLA pl 1.0` header line, then one line `<name> <x> <y> : N` per **movable** cell (lower-left = `center−(w/2,h/2)`); fixed cells either omitted or emitted at exact input coords (FR-005). Every movable cell written exactly once (FR-003).

**Checkpoint**: the binary can parse any of the 3 testcases (incl. `public2`'s 1201 terminals, negative cores) and write a well-formed (if not yet optimized) `.gp.pl`.

---

## Phase 3: User Story 1 - Legal placement for every testcase (Priority: P1) 🎯 MVP

**Goal**: produce a **legal** `.gp.pl` for all 3 testcases — every movable cell in-core, no fixed cell moved, and the placement **spread enough to pass the anti-collapse health check**.

**Independent Test**: run the binary on `public1..3`, then `python scorer/score.py 003 --output-dir IMPL/out` → every row `valid=OK` (SC-001) with `avgDisp ≤ 0.05×core` in the note (SC-006), each HPWL below its Max (SC-002).

- [ ] T011 [US1] Implement **constructive legal spreading** in `IMPL/main.cpp` (research §1/§2, data-model.md lifecycle step 1–2): distribute movable cells across the core so the placement is in-core and **uniformly spread** (e.g., assign cells to rows round-robin / by site capacity, or scatter on a coarse uniform grid sized to core area). This guarantees a legal start that trivially passes the Tetris health check (`avgDisp` small) even before any optimization.
- [ ] T012 [US1] Implement the **in-core clamp** in `IMPL/main.cpp` (research §7, FR-004): clamp each movable cell's center so `xmin ≤ x`, `x+w ≤ xmax`, `ymin ≤ y`, `y+h ≤ ymax` (lower-left bbox), applied before the writer. Verify it never moves fixed cells (FR-005).
- [ ] T013 [US1] Wire the MVP end-to-end in `IMPL/main.cpp`: parse → constructive spread → clamp → compute HPWL → write `.gp.pl`; build to `D:\FSecret\hw4.exe` and run on `public1`
- [ ] T014 [US1] **Validate US1**: generate outputs for all 3 cases and run `python scorer/score.py 003 --output-dir IMPL/out --label <model>`; confirm 3/3 `valid=OK`, record each case's HPWL vs **Max** and the note's `avgDisp` (must be `≤0.05×core`) (SC-001/SC-002/SC-006)

**Checkpoint**: a legal, spread placement for every case — the non-negotiable gate is met (likely high HPWL; US2 lowers it).

---

## Phase 4: User Story 2 - Minimize HPWL to beat the baseline (Priority: P2)

**Goal**: drive HPWL down via **analytical placement** (WL surrogate + density penalty) to **≤ Reference** then toward **≤ Min**, while the density term keeps the placement spread (legal). All tasks edit `IMPL/main.cpp` → sequential.

**Independent Test**: score all 3 cases; HPWL `≤ Reference` on every case and `<` on ≥1 (SC-004), aiming `≤ Min` (SC-003); all still `valid=OK` with `avgDisp ≤ 0.05×core`.

- [ ] T015 [US2] Implement the **wirelength surrogate** `f_WL` + gradient in `IMPL/main.cpp` (research §3): **WA** model preferred (`Σ x·e^{x/γ}/Σe^{x/γ}` per axis per net), **LSE** acceptable fallback (`γ·log Σ e^{x/γ}`); smoothing `γ ≈ coreW/10`; gradient w.r.t. each movable cell center; iterate over nets via the CSR arrays
- [ ] T016 [US2] Implement the **bell-shaped bin-density** penalty `f_D` + gradient in `IMPL/main.cpp` (research §4, data-model.md BinGrid): bin grid over the core, `targetDensity = Σ movable area / coreArea`, piecewise-quadratic θ_x·θ_y smoothing kernel, `f_D = Σ_bins (binDensity − targetDensity)²` and its gradient scattered to cell centers; rebuild `binDensity[]` each evaluation
- [ ] T017 [US2] Implement the combined objective + **own conjugate-gradient** solver in `IMPL/main.cpp` (research §3/§6/§10): `f = f_WL + λ·f_D`, `g = ∇f_WL + λ·∇f_D`; CG with line search and a step-size bound `≈ coreW`; fixed cells excluded from variables (held constant in WL/density)
- [ ] T018 [US2] Implement **WL-aware init + λ-ramp outer loop** in `IMPL/main.cpp` (research §4/§5/§9): init movable centers at core center; round 0 = λ=0 WL-only warmup; then ramp λ up each round (penalty schedule), CG-minimizing each round; **clamp in-core** after each round; stop when spread is sufficient (estimated/target density met, equiv. `avgDisp` margin) or the time guard fires, keeping best-so-far. Output the best spread placement.
- [ ] T019 [US2] **Validate US2**: score all 3 cases; confirm `valid=OK` + `avgDisp ≤ 0.05×core`, and record HPWL vs **Reference** and **Min**. Tune `γ`, λ schedule, and bin count so HPWL `≤ Reference` on every case and as close to **Min** as possible (SC-003/SC-004). If **all** cases are worse than Reference → trigger the R1 gate (T024).

**Checkpoint**: legal placements that beat the human Reference (R1 = keep self-written), approaching Min.

---

## Phase 5: User Story 3 - Converge within the runtime budget at scale (Priority: P3)

**Goal**: make the analytical solver **fast and parallel** (R2) so the largest case (`public3`, 51k cells) finishes within ~590s while reaching Min, using the reference's unused time slack. All tasks edit `IMPL/main.cpp` → sequential edits, **runtime-parallel** execution.

**Independent Test**: run `public3` → completes ≤ ~590s, exit 0, `valid=OK` (SC-005); all 3 cases still legal and at/under their targets.

- [ ] T020 [US3] **OpenMP-parallelize the objective/gradient** in `IMPL/main.cpp` (research §6): `#pragma omp parallel for` over nets (WL) and over bins×cells (density) with **thread-local partial accumulators reduced in fixed order** (determinism, FR-013); verify single-thread vs multi-thread HPWL match within FP tolerance and legality is identical
- [ ] T021 [US3] Implement **size-adaptive bin resolution** + the **wall-clock guard** in `IMPL/main.cpp` (research §4/§9): scale `binCutX/Y` to design size (finer than the reference's fixed 14×14, e.g. `≈√(coreArea/(k·avgCellArea))` or bins a few rows tall); the outer loop checks elapsed vs the ~560s deadline each round and exits early keeping best-so-far
- [ ] T022 [US3] (Optional, quality hedge) Implement **parallel multi-start** in `IMPL/main.cpp` (research §6): a few chains with diverse `γ`/seed/bin schedules (seed = chain index), each producing a clamped spread placement; reduce to the best **legal** result tie-broken by `(scorer-HPWL, chain index)`
- [ ] T023 [US3] **Validate US3**: time all 3 cases (esp. `public3`); confirm each ≤ ~590s, exit 0, `valid=OK`, and HPWL at/under target; re-run `public3` twice to confirm deterministic legality (FR-013, SC-005)

**Checkpoint**: all cases legal, at/under Min where achievable, within budget — the full feature.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: R1 decision, determinism, and reporting.

- [ ] T024 **R1 Baseline Fallback gate** (research §10, constitution R1): only if T019 found **all 3** cases worse than README **Reference** — re-implement the reference *algorithm* inside `IMPL/main.cpp` (LSE WL + bell density + λ ramp exactly per [`reference/src/ExampleFunction.cpp`](../../../../../problems/003-global-placement/reference/src/ExampleFunction.cpp) / [`GlobalPlacer.cpp`](../../../../../problems/003-global-placement/reference/src/GlobalPlacer.cpp), **self-contained**, not linking `reference/obj`), then re-apply T021/T022 tuning. Keep self-written code if it beats Reference on **any** case. (Skip this task if T019 already beats Reference anywhere.)
- [ ] T025 [P] Determinism + robustness pass on `IMPL/main.cpp`: confirm fixed thread count + fixed-order reductions; re-run all cases to confirm identical legality and equal-or-better HPWL (FR-013); confirm graceful handling of negative cores and degenerate nets
- [ ] T026 [P] Write `IMPL/RESULT.md` from the template in `experiments/README.md`: per-case `valid` / HPWL / runtime / `avgDisp`, vs Min/Reference/Max; build & run commands; rounds and R1 decision (self-written vs ported)
- [ ] T027 **Final all-case gate** (R3/R4): one clean `python scorer/score.py 003 --output-dir IMPL/out --label <model>` run showing 3/3 `valid=OK`, all HPWL `< Max` (and the count `≤ Reference` / `≤ Min`), confirming the spec's Success Criteria

---

## Dependencies & completion order

- **Phase 1 (Setup)** → **Phase 2 (Foundational)** block everything.
- **US1 (P1)** depends only on Phase 2 → the MVP (legal spread placement). Independently shippable.
- **US2 (P2)** depends on Phase 2 + US1's clamp/writer/HPWL → lowers HPWL. Independently scorable.
- **US3 (P3)** depends on US2's objective/solver → parallelizes & fits the budget. Independently scorable.
- **Polish** depends on US2 (R1 gate) and US3 (final numbers).
- Story order = priority order P1 → P2 → P3; each phase ends `valid=OK` on all 3 cases (R3/R4).

## Parallel opportunities

- **T002** ([P], `IMPL/Makefile`) parallel with T001.
- **T025 / T026** ([P]) parallel in Polish (separate concerns; T026 writes `RESULT.md`, not `main.cpp`).
- All other tasks edit the single `IMPL/main.cpp` → **sequential**. The real parallelism is **runtime** (US3: OpenMP FG + multi-start), not in this task graph.

## Implementation strategy

- **MVP = Phase 1 + Phase 2 + US1** → a legal, spread `.gp.pl` for all 3 cases (passes the hard gate, incl. anti-collapse). Stop here and you have a valid-but-unoptimized submission.
- **Incremental**: add US2 (beat Reference / approach Min) → add US3 (parallel + budget at 51k cells) → Polish (R1 decision, determinism, RESULT.md).
- **Every change**: score **all 3** cases (R4) and watch **both** `valid` **and** `avgDisp` (R3 + anti-collapse). The scorer is the sole arbiter (R6).
