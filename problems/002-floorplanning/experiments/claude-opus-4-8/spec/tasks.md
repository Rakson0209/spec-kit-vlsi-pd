---
description: "Task list for Fixed-Outline Floorplanning implementation"
---

# Tasks: Fixed-Outline Floorplanning

**Input**: Design documents in `problems/002-floorplanning/experiments/claude-opus-4-8/spec/`
**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md), [data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md)

## Conventions (read first)

- **`IMPL/` = `problems/002-floorplanning/experiments/<model>/`** — the implementing model's directory. **All code paths below are relative to `IMPL/`.** For opus's own implementation, `<model>` = `claude-opus-4-8`. (This `tasks.md` is shared: it is copied into each model's `spec/` and each model implements its own `IMPL/main.cpp`.)
- **Single translation unit**: the whole solver lives in `IMPL/main.cpp` (plan.md). Therefore **most tasks edit the same file and are sequential** — `[P]` is used only for genuinely separate files. The real parallelism is at **runtime** (OpenMP/pthread multi-start, T016), not in this task graph.
- **Testing = the scorer** (constitution R6). No separate unit-test code is generated; each story's "validation" task runs `scorer/score.py` (the project's acceptance test). Tests were **not** requested in the spec.
- **Build/run/score commands**: see [quickstart.md](./quickstart.md). Build flags are fixed: `g++ -std=c++20 -O3 -fopenmp -pthread` (**no Boost**, constitution R2; the reference is Boost-free too). **Exec constraint**: the compiled `hw3.exe` must live under **`D:\FSecret\`** to run (Defender ASR); output files may live anywhere.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: different file, no dependency on an incomplete task → may run in parallel
- **[Story]**: `[US1]`/`[US2]`/`[US3]` for user-story phases (Setup/Foundational/Polish carry no story label)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: scaffold the experiment directory and confirm the toolchain builds.

- [ ] T001 Create `IMPL/main.cpp` skeleton (`int main(int argc,char**argv)`) and the `IMPL/out/` output directory
- [ ] T002 [P] Create `IMPL/Makefile` with target `g++ -std=c++20 -O3 -fopenmp -pthread -o hw3 main.cpp` (no `-I boost`)
- [ ] T003 Load toolchain (`. .\tools\mingw64\setup-env.ps1`, or reload PATH per [quickstart.md](./quickstart.md)), build the skeleton to `D:\FSecret\hw3.exe`, and confirm it runs and accepts two path args (per [contracts/cli-contract.md](./contracts/cli-contract.md))

**Checkpoint**: empty binary builds with the project flags and runs from `D:\FSecret\`.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: the I/O + data + geometry layer every story depends on. **No user story can begin until this is done.** All tasks edit `IMPL/main.cpp` → sequential.

- [ ] T004 Implement CLI handling + deadline in `IMPL/main.cpp`: read `argv[1]` (input) / `argv[2]` (output), create the output's parent dir if missing, capture a wall-clock start (`omp_get_wtime()` / `std::chrono::steady_clock`) and set an internal deadline `< 600s`; exit 0 on success (per [contracts/cli-contract.md](./contracts/cli-contract.md))
- [ ] T005 Implement the input parser in `IMPL/main.cpp` per [contracts/io-format.md](./contracts/io-format.md): `getline`+whitespace tokenize (skip blank lines); parse `ChipSize W H`, `NumSoftModules`/`SoftModule <name> <area>`, `NumFixedModules`/`FixedModule <name> <x> <y> <w> <h>`, `NumNets`/`Net <A> <B> <weight>`; **intern module names → contiguous int ids** in one id space over soft+fixed, with an `id→name` table and an `isFixed[]` flag (data-model.md §A/§B)
- [ ] T006 Build the in-memory model in `IMPL/main.cpp` (data-model.md §B): module arrays `x,y,w,h,fixed,area`; per-soft **candidate-shape generation** (research §4 — sweep `w∈[⌈√(A/2)⌉,⌊√(2A)⌋]`, `h=⌈A/w⌉`, **verify** `w·h≥area` and `0.5≤h/w≤2` on every candidate, always include the near-square); net array `(a,b,wt)` as ids; per-module incident-net lists; **precompute fixed-module centers** (constants)
- [ ] T007 Implement the grid-free geometry primitives in `IMPL/main.cpp` (research §3, data-model.md §B): `overlap(i,j)` with **strict** inequalities (exactly the scorer's `_overlap`; edge-touch legal), `inOutline(i)`, `legalPlacement(i)` (in-outline + `w·h≥area` + `h/w∈[0.5,2]` + no overlap over soft+fixed), and `compactDown/Left(i)` via `O(n)` rectangle maxima. **No `H×W` grid.**
- [ ] T008 Implement wirelength in `IMPL/main.cpp` (research §8/§10): center `(x + w/2, y + h/2)` integer **floor**; `totalWL = Σ wt·(|Δcx|+|Δcy|)` over nets as `long long` (== scorer); plus an **incremental delta** that recomputes only a single moved module's incident nets
- [ ] T009 Implement the output writer in `IMPL/main.cpp` per [contracts/io-format.md](./contracts/io-format.md): emit optional `Wirelength <totalWL>`, then `NumSoftModules <n>`, then one line `name x y w h` per **soft** module (from `id→name`); **fixed modules are never written**; the self-reported `Wirelength` must equal the recomputed value (FR-010)

**Checkpoint**: the binary can parse any of the 5 testcases and write a well-formed (if not yet optimized) output file.

---

## Phase 3: User Story 1 - Legal floorplan for every testcase (Priority: P1) 🎯 MVP

**Goal**: produce a **legal** `.floorplan` for all 5 testcases (every soft module shaped + placed in-outline, no overlap with any module, area/ratio satisfied).

**Independent Test**: run the binary on `sample` + `public1..4`, then `python scorer/score.py 002 --output-dir IMPL/out` → every row `valid=OK` (SC-001), each public wirelength below its Max (SC-002).

- [ ] T010 [US1] Implement the **constructive bottom-left packing** in `IMPL/main.cpp` (research §5, data-model.md legality invariants): mark fixed modules as obstacles, place soft modules in area-descending order, each at the lowest-then-leftmost legal position (via the T007 compaction primitive) with a fitting legal shape; on placement failure, retry with a different order/shape; guarantee in-outline + non-overlap + area + ratio (FR-003..FR-008). Must succeed even at `public2`'s ~93% density.
- [ ] T011 [US1] Wire the end-to-end path in `IMPL/main.cpp`: parse → shape gen → constructive pack → compute wirelength → write output; build to `D:\FSecret\hw3.exe` and run on `sample`, confirming the output format and that both `GPU`/`CPU` are placed legally around `PAD1`/`FIXED1`
- [ ] T012 [US1] **Validate US1**: generate outputs for all 5 cases and run `python scorer/score.py 002 --output-dir IMPL/out --label <model>`; confirm 5/5 `valid=OK` and record each wirelength vs Max (SC-001/SC-002)

**Checkpoint**: MVP delivered — every testcase is legal. Wirelength is whatever the constructive pack yields (typically < Max, not yet ≤ Reference).

---

## Phase 4: User Story 2 - Minimize weighted HPWL to beat the baseline (Priority: P2)

**Goal**: drive wirelength down — at minimum ≤ Reference on every case (R1 gate), toward ≤ Min. All tasks edit `IMPL/main.cpp` → sequential; parallelism is at runtime (T016).

**Independent Test**: full-case scorer run; wirelength ≤ Reference on all cases and `<` on ≥1 (SC-004), with measured gap to Min (SC-003).

- [ ] T013 [US2] Implement **weighted-median coordinate descent** in `IMPL/main.cpp` (research §6a): for each soft module compute the wirelength-optimal center as the **weighted median** of its incident-net neighbors' centers (x and y separately, weights = net weights); move the module so its center reaches the target (clamp to outline, snap to integers); if the direct move overlaps, compact toward the target / take the closest legal position; accept iff `totalWL` does not increase; sweep modules until none improves. **Pure wirelength** (no area term).
- [ ] T014 [US2] Implement the **simulated-annealing engine** in `IMPL/main.cpp` (research §6b, data-model.md "State transition"): moves {translate to a nearby/random legal spot, `swap(i,j)` positions (reshaping to fit), reshape within the candidate set, nudge-toward-median}; **pure-WL cost**; **incremental HPWL** (T008) on the moved module's incident nets; legality via T007; Metropolis acceptance with geometric cooling; always track/restore the best **legal** solution
- [ ] T015 [US2] Combine into a **memetic loop** in `IMPL/main.cpp` (research §6): interleave SA exploration with periodic median-descent intensification; ensure the best legal solution is retained across both engines
- [ ] T016 [US2] Implement **parallel multi-start** in `IMPL/main.cpp` (research §7, constitution R2): run M chains (`#pragma omp parallel` / `std::thread`) of (diversified init order + SA + median descent), **seed = chain index** (FR-015), then reduce to the best **legal** result (lowest wirelength, tie → lowest chain index)
- [ ] T017 [US2] **Validate US2**: full-case scorer run; confirm wirelength ≤ Reference on all and `<` on ≥1 (SC-004 / R1 gate); record gap to Min per case (SC-003)

**Checkpoint**: self-written solver beats the human baseline → R1 keeps it (no fallback). Looser cases (`public3/4`) approach Min.

---

## Phase 5: User Story 3 - Converge to a strong packing within the runtime budget (Priority: P3)

**Goal**: every case finishes within ~600s while staying legal and low-wirelength — especially `public1` (largest outline, ref 581s) and the dense `public2`. All tasks edit `IMPL/main.cpp` → sequential.

**Independent Test**: run `public1..4`; each completes ≤ ~600s with `valid=OK` and a low wirelength (aim ≤ Min) (SC-005/SC-003).

- [ ] T018 [US3] Integrate the **wall-clock deadline** across init/SA/median/multi-start in `IMPL/main.cpp` (research §9): check `omp_get_wtime()` against an internal cutoff `< 600s` (e.g. ~575–590s); on expiry, stop and emit the best legal solution found so far (protects R3/SC-005)
- [ ] T019 [US3] Tune the **iteration/restart budget** in `IMPL/main.cpp` (research §9): scale chain count M and SA iteration counts so the hardest cases converge comfortably under the deadline; confirm the grid-free design lets `public1` (15 huge blocks, chip `11267×10450`) finish `< 600s`; keep stopping iteration-budget-driven (deadline only truncates extra restarts) so re-runs are reproducible (FR-015)
- [ ] T020 [US3] Harden **dense-case legality** in `IMPL/main.cpp` for `public2` (~93% utilization): ensure constructive init + reshape/swap moves can find and maintain a legal packing when slack is tiny, and that every multi-start chain returns a legal solution (R3)
- [ ] T021 [US3] **Validate US3**: run `public1..4` (especially `public1`); confirm each ≤ ~600s, `valid=OK`, and wirelength as low as possible (aim ≤ Min) (SC-005/SC-003)

**Checkpoint**: all cases legal, within budget, and competitive with Min across the dense / huge-block / many-DOF regimes.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: baseline decision, full validation, determinism, cleanup.

- [ ] T022 Run the full 5-case scorer with reports: `python scorer/score.py 002 --output-dir IMPL/out --label <model> --md IMPL/score.md --csv IMPL/score.csv`; record wirelength + runtime vs Min/Reference/Max in `IMPL/RESULT.md` (constitution R4, spec SC mapping)
- [ ] T023 **R1 Baseline Fallback decision** (constitution R1, research §11): if **every** case is worse than Reference, copy `reference/src/main.cpp` into `IMPL/`, build with the project flags (it is **Boost-free** — no porting needed), then optimize on top (drop its `alpha·area` cost term → **pure wirelength**; add **parallel multi-start**; optionally replace the `H×W` grid with rectangle checks); **otherwise keep the self-written solver** (any single case beating Reference suffices)
- [ ] T024 [P] Determinism check (FR-015): re-run two cases and confirm equal-or-better wirelength and identical legality; note results in `IMPL/RESULT.md`
- [ ] T025 Final cleanup of `IMPL/main.cpp`: strip debug logging from hot paths, confirm no Boost include, confirm a clean build with `g++ -std=c++20 -O3 -fopenmp -pthread`, verify the self-reported `Wirelength` equals the scorer on all cases (SC-006), and walk through [quickstart.md](./quickstart.md) end-to-end

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (P1)** → no deps; start immediately.
- **Foundational (P2)** → after Setup; **blocks all user stories**.
- **US1 (P3)** → after Foundational. Delivers the MVP (legal output) using the constructive packer + writer.
- **US2 (P4)** → after Foundational; builds on the US1 constructive init (its legal packing seeds the optimizer) and the shared geometry/wirelength infrastructure.
- **US3 (P5)** → after Foundational; tunes the US2 engine (deadline, budget, dense-case robustness), so it is most valuable **after** US2.
- **Polish (P6)** → after the desired stories; T023 (R1 decision) needs US2/US3 results.

### Within each story

- US1: constructive pack (T010) → end-to-end wire (T011) → validate (T012).
- US2: median descent (T013) → SA engine (T014) → memetic combine (T015) → multi-start (T016) → validate (T017).
- US3: deadline (T018) → budget tuning (T019) → dense-case hardening (T020) → validate (T021).

### Story independence (per spec)

- **US1** is independently testable (legality on all 5 cases) with only Setup + Foundational + T010–T011.
- **US2** and **US3** each have their own scorer-based validation (T017, T021) and degrade gracefully: shipping only US1 still passes legality; shipping US1+US2 still beats Reference on the looser cases even before US3's budget tuning.

---

## Parallel Opportunities (limited — single file)

- **T002** (`Makefile`) is `[P]` vs T001 (`main.cpp`) — different files.
- **T024** (determinism re-run) is `[P]` — read-only validation, independent of T023/T025 edits.
- **All other code tasks share `IMPL/main.cpp` and are sequential.** The intended parallelism is at **runtime**: T016's OpenMP/pthread multi-start (constitution R2), not concurrent editing.

---

## Implementation Strategy

### MVP first (US1 only)

1. Phase 1 Setup → 2. Phase 2 Foundational → 3. Phase 3 US1 → **STOP & VALIDATE**: `score.py` shows 5/5 `valid=OK`. A shippable, legal baseline (SC-001/SC-002).

### Incremental delivery

1. Setup + Foundational → I/O + geometry + wirelength layer ready.
2. + US1 → legal on all cases (**MVP**).
3. + US2 (median descent + SA + multi-start, pure-WL) → beats Reference (R1 gate cleared), approaches Min on the looser cases.
4. + US3 (deadline + budget tuning + dense-case hardening) → `public1` fast, `public2` legal, all near-Min.
5. Polish → R1 decision, full report, determinism, cleanup.

After **each** code change, re-score **all** cases (constitution R4) — never tune one case in isolation.

---

## Notes

- `[P]` = different file, no incomplete-task dependency. `[Story]` maps a task to its user story for traceability.
- The scorer (`scorer/lib/floorplanning.py`) is the sole arbiter of legality and wirelength (R6); match its **strict-inequality** overlap, integer **floor** centers `(x+w//2, y+h//2)`, area-as-lower-bound, and `h/w∈[0.5,2]±1e-9` exactly.
- The objective is **pure weighted HPWL** — never put area in the cost (research §2); area/outline/ratio are constraints only.
- Keep a legal solution in hand at all times so a deadline/timeout still yields a scorable output (R3).
- Build `hw3.exe` under `D:\FSecret\` to run it (exec constraint); commit after each task or logical group.
