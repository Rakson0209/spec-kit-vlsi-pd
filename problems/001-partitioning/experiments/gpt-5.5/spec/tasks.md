---
description: "Task list for Multi-Technology Die Partitioning implementation"
---

# Tasks: Multi-Technology Die Partitioning

**Input**: Design documents in `problems/001-partitioning/experiments/claude-opus-4-8/spec/`
**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md), [data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md)

## Conventions (read first)

- **`IMPL/` = `problems/001-partitioning/experiments/<model>/`** — the implementing model's directory. **All code paths below are relative to `IMPL/`.** For opus's own implementation, `<model>` = `claude-opus-4-8`. (This `tasks.md` is shared: it is copied into each model's `spec/` and each model implements its own `IMPL/main.cpp`.)
- **Single translation unit**: the whole solver lives in `IMPL/main.cpp` (plan.md). Therefore **most tasks edit the same file and are sequential** — `[P]` is used only for genuinely separate files. The real parallelism is at **runtime** (OpenMP/pthread multi-start, T017), not in this task graph.
- **Testing = the scorer** (constitution R6). No separate unit-test code is generated; each story's "validation" task runs `scorer/score.py` (the project's acceptance test). Tests were **not** requested in the spec.
- **Build/run/score commands**: see [quickstart.md](./quickstart.md). Build flags are fixed: `g++ -std=c++20 -O3 -fopenmp -pthread` (**no Boost**, constitution R2).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: different file, no dependency on an incomplete task → may run in parallel
- **[Story]**: `[US1]`/`[US2]`/`[US3]` for user-story phases (Setup/Foundational/Polish carry no story label)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: scaffold the experiment directory and confirm the toolchain builds.

- [X] T001 Create `IMPL/main.cpp` skeleton (`int main(int argc,char**argv)`) and the `IMPL/out/` output directory
- [X] T002 [P] Create `IMPL/Makefile` with target `g++ -std=c++20 -O3 -fopenmp -pthread -o hw2 main.cpp` (no `-I boost`)
- [X] T003 Load toolchain (`. .\tools\mingw64\setup-env.ps1`), build the skeleton, and confirm `IMPL/hw2.exe` runs and accepts two path args (per [contracts/cli-contract.md](./contracts/cli-contract.md))

**Checkpoint**: empty binary builds and runs with the project flags.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: the I/O + data layer every story depends on. **No user story can begin until this is done.** All tasks edit `IMPL/main.cpp` → sequential.

- [X] T004 Implement CLI handling + deadline in `IMPL/main.cpp`: read `argv[1]` (input) / `argv[2]` (output), create the output's parent dir if missing, capture a `std::chrono::steady_clock` start and set a ~285s deadline; exit 0 on success (per [contracts/cli-contract.md](./contracts/cli-contract.md))
- [X] T005 Implement the input parser in `IMPL/main.cpp` per [contracts/io-format.md](./contracts/io-format.md): buffered whole-file read + whitespace tokenize (skip blank lines); parse `NumTechs`/`Tech`/`LibCell` into per-Tech `libArea`, `DieSize`, `DieA`/`DieB` (tech + `util%/100`), `NumCells`/`Cell`, `NumNets`/`Net` membership; **intern cell names → contiguous int ids** with an `id→name` table (data-model.md §A/§B)
- [X] T006 Build the in-memory model in `IMPL/main.cpp` (data-model.md §B): per-cell `areaA`/`areaB` precompute from the two dies' Techs; CSR adjacency `cellPins` and `netPins` (`values[]`+`offset[]`); die physical area `W·H` and cap fractions
- [X] T007 Implement the output writer in `IMPL/main.cpp` (per [contracts/io-format.md](./contracts/io-format.md)): emit optional `CutSize <n>`, then `DieA <countA>` + one cell name per line, then `DieB <countB>` + names, from `side[]` and the `id→name` table; counts must equal the listed lines
- [X] T008 Implement cut-size computation in `IMPL/main.cpp`: count nets with `countA>0 && countB>0` (== scorer metric, research §2/§9); reused for the `CutSize` line and best-solution tracking

**Checkpoint**: the binary can parse any of the 7 testcases and write a well-formed (if not yet optimized) output file.

---

## Phase 3: User Story 1 - Legal partition for every testcase (Priority: P1) 🎯 MVP

**Goal**: produce a **legal** `.out` for all 7 testcases (every cell assigned once; both dies within their utilization caps).

**Independent Test**: run the binary on `sample` + `public1..6`, then `python scorer/score.py 001 --output-dir IMPL/out` → every row `valid=OK` (SC-001), each public cut below its Max (SC-002).

- [X] T009 [US1] Implement the **feasibility-first greedy initial partition** in `IMPL/main.cpp` (research §3, data-model.md §A "Die" invariant): assign each cell to the relatively-cheaper die (`areaA/capA` vs `areaB/capB`) if the destination cap still allows (`used + areaDest ≤ cap·area + 1e-9`), else the other die; maintain `usedArea[A]`/`usedArea[B]`; guarantee full exclusive coverage (FR-003/FR-004)
- [X] T010 [US1] Wire the end-to-end path in `IMPL/main.cpp`: parse → greedy init → compute cut → write output; build and run on `sample`, confirming the output format and that all 8 cells appear once
- [X] T011 [US1] **Validate US1**: generate outputs for all 7 cases and run `python scorer/score.py 001 --output-dir IMPL/out --label <model>`; confirm 7/7 `valid=OK` and record each cut vs Max (SC-001/SC-002)

**Checkpoint**: MVP delivered — every testcase is legal. Cut size is whatever greedy yields (typically ≪ Max, not yet ≤ Reference).

---

## Phase 4: User Story 2 - Minimize cut size to beat the baseline targets (Priority: P2)

**Goal**: drive cut size down — at minimum ≤ Reference on every case (R1 gate), toward ≤ Min. All tasks edit `IMPL/main.cpp` → sequential; parallelism is at runtime (T017).

**Independent Test**: full-case scorer run; cut ≤ Reference on all cases and `<` on ≥1 (SC-004), with measured gap to Min (SC-003).

- [X] T012 [US2] Maintain net side-counts `countA[n]`/`countB[n]` in `IMPL/main.cpp` during init and every move (data-model.md §B; basis for gains and cut)
- [X] T013 [US2] Implement initial FM gain in `IMPL/main.cpp`: `gain(c) = (#incident nets with F(n)==1) − (#incident nets with T(n)==0)` (research §2)
- [X] T014 [US2] Implement the **bucket-list gain structure** in `IMPL/main.cpp`: array indexed by gain `[−maxDeg, +maxDeg]` of doubly-linked lists + a `maxGainPtr`; O(1) max-extract and O(1) per-neighbor update (research §6, data-model.md §B)
- [X] T015 [US2] Implement one **FM pass** in `IMPL/main.cpp` (research §2, data-model.md "State transition"): repeatedly take the max-gain unlocked cell whose move is feasible (`dest used + areaDest ≤ cap·area + 1e-9`), move+lock it, update `countA/countB`, areas, and neighbor gains incrementally, and track the cumulative-gain **prefix maximum**
- [X] T016 [US2] Implement **roll-back-to-best-prefix** + multi-pass loop in `IMPL/main.cpp`: after each pass revert moves past the best prefix, unlock cells, repeat until stall (best-prefix gain ≤ 0 or ≤ 1 move) or deadline
- [X] T017 [US2] Implement **parallel multi-start** in `IMPL/main.cpp` (research §4, constitution R2): run K restarts (`#pragma omp parallel` / `std::thread`) of randomized greedy init + FM, seed = restart index (FR-013), reduce to the best **legal** result (lowest cut, tie → lowest index)
- [X] T018 [US2] **Validate US2**: full-case scorer run; confirm cut ≤ Reference on all and `<` on ≥1 (SC-004 / R1 gate); record gap to Min per case (SC-003)

**Checkpoint**: self-written solver beats the human baseline → R1 keeps it (no fallback). Small/medium cases approach Min.

---

## Phase 5: User Story 3 - Scale to the largest testcases within the runtime budget (Priority: P3)

**Goal**: handle `public3/5/6` (up to ~4.15M lines) within ~300s while staying legal and low-cut. All tasks edit `IMPL/main.cpp` → sequential.

**Independent Test**: run `public3`, `public5`, `public6`; each completes ≤ ~300s with `valid=OK` and a low cut (aim ≤ Min) (SC-005/SC-003).

- [X] T019 [US3] Performance-harden parsing/layout in `IMPL/main.cpp` for ~4M-line inputs (research §7): ensure the buffered parse, reserved maps, and CSR arrays let `public6` parse in seconds and keep memory bounded (FR-011)
- [X] T020 [US3] Implement **multilevel coarsening** in `IMPL/main.cpp` (research §5, data-model.md §B hierarchy): cluster strongly-connected cells via heavy-edge / first-choice matching, accumulate per-Tech cluster area, build a level stack down to a few-thousand vertices
- [X] T021 [US3] Implement **uncoarsening + per-level FM refinement** in `IMPL/main.cpp`: partition the coarsest level (greedy + multi-start FM), then project down level-by-level and run the T015/T016 FM refinement at each level (research §5)
- [X] T022 [US3] Add the **size threshold gate** in `IMPL/main.cpp`: use multilevel when `|cells|` exceeds ~50k, else the flat multi-start FM path; ensure both paths always yield a legal partition
- [X] T023 [US3] Integrate the **~285s wall-clock deadline** across init/FM/multilevel/multi-start in `IMPL/main.cpp` (research §8): on expiry, stop and emit the best legal partition found so far (protects R3/SC-005)
- [X] T024 [US3] **Validate US3**: run `public3/5/6`; confirm each ≤ ~300s, `valid=OK`, and cut as low as possible (aim ≤ Min) (SC-005/SC-003)

**Checkpoint**: all 7 cases legal, fast, and competitive with Min across the size range.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: baseline decision, full validation, determinism, cleanup.

- [X] T025 Run the full 7-case scorer with reports: `python scorer/score.py 001 --output-dir IMPL/out --label <model> --md IMPL/score.md --csv IMPL/score.csv`; record cut + runtime vs Min/Reference/Max in `IMPL/RESULT.md` (constitution R4, spec SC mapping)
- [X] T026 **R1 Baseline Fallback decision** (constitution R1, research §10): if **every** case is worse than Reference, copy `reference/src/main.cpp` into `IMPL/`, port it off Boost (`boost::unordered_map`→`std::unordered_map`; remove `boost/fusion` includes and `using namespace boost`; add `<unordered_map>`/`<map>`/`<list>`), build with the project flags (no `-I boost`), then optimize on top; **otherwise keep the self-written solver** (any single case beating Reference suffices)
- [X] T027 [P] Determinism check (FR-013): re-run two cases and confirm equal-or-better cut and identical legality; note results in `IMPL/RESULT.md`
- [X] T028 Final cleanup of `IMPL/main.cpp`: strip debug logging from hot paths, confirm no Boost include remains, confirm a clean build with `g++ -std=c++20 -O3 -fopenmp -pthread`, and walk through [quickstart.md](./quickstart.md) end-to-end

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (P1)** → no deps; start immediately.
- **Foundational (P2)** → after Setup; **blocks all user stories**.
- **US1 (P3)** → after Foundational. Delivers the MVP (legal output).
- **US2 (P4)** → after Foundational; builds on the US1 init (uses the greedy partition as FM's start) and the shared cut/count infrastructure.
- **US3 (P5)** → after Foundational; reuses the US2 FM engine (T015/T016) as its per-level refiner, so US3 is most valuable **after** US2, though its parser hardening (T019) is independent.
- **Polish (P6)** → after the desired stories; T026 (R1 decision) needs US2/US3 results.

### Within each story

- Counts (T012) → gains (T013) → buckets (T014) → FM pass (T015) → roll-back/loop (T016) → multi-start (T017) → validate (T018).
- Coarsen (T020) → uncoarsen+refine (T021) → threshold gate (T022) → deadline (T023) → validate (T024).

### Story independence (per spec)

- **US1** is independently testable (legality on all 7 cases) with only Setup + Foundational + T009–T010.
- **US2** and **US3** each have their own scorer-based validation (T018, T024) and degrade gracefully: shipping only US1 still passes legality; shipping US1+US2 (no multilevel) still beats Reference on small/medium cases.

---

## Parallel Opportunities (limited — single file)

- **T002** (`Makefile`) is `[P]` vs T001 (`main.cpp`) — different files.
- **T027** (determinism re-run) is `[P]` — read-only validation, independent of T026/T028 edits.
- **All other code tasks share `IMPL/main.cpp` and are sequential.** The intended parallelism is at **runtime**: T017's OpenMP/pthread multi-start and parallel gain/coarsening (constitution R2), not concurrent editing.

---

## Implementation Strategy

### MVP first (US1 only)

1. Phase 1 Setup → 2. Phase 2 Foundational → 3. Phase 3 US1 → **STOP & VALIDATE**: `score.py` shows 7/7 `valid=OK`. This is a shippable, legal baseline (SC-001/SC-002).

### Incremental delivery

1. Setup + Foundational → I/O layer ready.
2. + US1 → legal on all cases (**MVP**).
3. + US2 (FM + multi-start) → beats Reference (R1 gate cleared), approaches Min on small cases.
4. + US3 (multilevel + deadline) → large cases fast and near-Min.
5. Polish → R1 decision, full report, determinism, cleanup.

After **each** code change, re-score **all** cases (constitution R4) — never tune one case in isolation.

---

## Notes

- `[P]` = different file, no incomplete-task dependency. `[Story]` maps a task to its user story for traceability.
- The scorer (`scorer/lib/partitioning.py`) is the sole arbiter of legality and cut (R6); match its `≤ cap + 1e-9` and side-dependent area semantics exactly.
- Keep a legal partition in hand at all times so a deadline/timeout still yields a scorable output (R3).
- Commit after each task or logical group.
