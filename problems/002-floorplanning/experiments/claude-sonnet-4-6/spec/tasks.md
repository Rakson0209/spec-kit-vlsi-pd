# Tasks: Fixed-outline Floorplanning Optimizer (claude-sonnet-4-6)

**Input**: Design documents from `problems/002-floorplanning/experiments/claude-sonnet-4-6/spec/`

**Prerequisites**: plan.md ✅ spec.md ✅ research.md ✅ data-model.md ✅ contracts/ ✅

**Implementation target**: `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
**Binary**: `problems/002-floorplanning/experiments/claude-sonnet-4-6/hw3`

**Organization**: Tasks are grouped by user story to enable independent validation of each story.

## Format: `[ID] [P?] [Story?] Description`

- **[P]**: Can be implemented in parallel (independent functions/sections)
- **[US?]**: Maps to user story (US1 = Valid Floorplan, US2 = HPWL Min, US3 = Runtime)

---

## Phase 1: Setup

**Purpose**: Create the project skeleton and verify the build pipeline.

- [x] T001 Create `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp` with `main(int argc, char** argv)` that accepts two CLI arguments (input file, output file) and exits gracefully if arguments are missing
- [x] T002 Verify compilation: run `g++ -std=c++11 -O3 -o hw3 main.cpp` from `problems/002-floorplanning/experiments/claude-sonnet-4-6/`; binary must produce without warnings

**Checkpoint**: Binary `hw3` compiles and runs with `./hw3 --help` or prints usage message.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Data structures and parser that ALL user stories depend on.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [x] T003 Define `Module` struct (fields: `int id, x, y, w, h, min_area; string name; bool is_fixed; vector<pair<int,int>> shapes;`) and `Net` struct (fields: `int a, b, weight;`) in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T004 Implement `parseInput(string filepath, int& chip_W, int& chip_H, vector<Module>& modules, vector<Net>& nets)` that reads the `.txt` format per [contracts/input-format.md](contracts/input-format.md): parses `ChipSize`, `SoftModule`/`FixedModule` sections (skipping blank lines), and `Net` section (resolving module names to indices via `unordered_map<string,int>`) in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T005 Implement `genShapes(int min_area, vector<pair<int,int>>& shapes)` that enumerates at most 20 valid (w, h) pairs satisfying `w*h >= min_area` and `0.5 <= (double)h/w <= 2.0` by sampling `w` in `[ceil(sqrt(min_area/2.0)), floor(sqrt(2.0*min_area))]` with step-size to yield ≤20 samples; call this for each soft module after parsing in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`

**Checkpoint**: Can parse `sample.txt` and print all module names/areas to stdout for verification.

---

## Phase 3: User Story 1 — Valid Floorplan Generation (Priority: P1) 🎯 MVP

**Goal**: Parse any testcase, produce a `.floorplan` file that passes the verifier (no overlaps, all modules within chip bounds, fixed modules unmoved).

**Independent Test**: Run `scorer/score.py` on sample.txt output → `valid: True`.

### Implementation for User Story 1

- [x] T006 [US1] Define `SequencePair` struct (`vector<int> pos_x, pos_y`) representing Γ⁺ and Γ⁻ permutations of soft module indices; implement `initSP(int n_soft, SequencePair& sp)` that initializes both as the identity permutation `{0,1,…,n_soft-1}` in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T007 [US1] Implement `buildHCG(const SequencePair& sp, const vector<Module>& softMods, vector<vector<pair<int,int>>>& hcg)`: for each pair (i,j) where soft module i appears before j in **both** `pos_x` and `pos_y`, add directed edge `i → j` with weight `softMods[i].w` to the horizontal constraint graph `hcg` (n nodes = n_soft + 2 sentinel nodes for source/sink) in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T008 [US1] Implement `buildVCG(const SequencePair& sp, const vector<Module>& softMods, vector<vector<pair<int,int>>>& vcg)`: same as T007 but vertical — edge `i → j` if i before j in `pos_x` AND j before i in `pos_y`, weight = `softMods[i].h` in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T009 [US1] Implement `longestPath(const vector<vector<pair<int,int>>>& graph, int n, vector<int>& dist)` using Kahn's algorithm (BFS topological sort + relaxation); `dist[j]` = longest distance from source to node j = x (or y) coordinate of module j in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T010 [US1] Implement `pack(const SequencePair& sp, vector<Module>& softMods, const vector<Module>& fixedMods, int chip_W, int chip_H, int& overflow_W, int& overflow_H)` that: (1) calls buildHCG → longestPath → sets `softMods[i].x`; (2) calls buildVCG → longestPath → sets `softMods[i].y`; (3) computes packed bounding box and sets `overflow_W = max(0, packed_W - chip_W)`, `overflow_H = max(0, packed_H - chip_H)` in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T011 [US1] Add fixed module handling inside `pack()`: after computing soft module positions via sequence pair, check each soft module against each fixed module for rectangle overlap (`ax < bx+bw && bx < ax+aw && ay < by+bh && by < ay+ah`); if overlap detected, shift the soft module to the right of or above the fixed module (simple greedy resolution) in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T012 [US1] Implement `writeFloorplan(const string& filepath, const vector<Module>& softMods, long long hpwl)` that writes the output per [contracts/output-format.md](contracts/output-format.md): line 1 `Wirelength <hpwl>`, blank line, `NumSoftModules <n>`, then one `<name> <x> <y> <w> <h>` line per soft module in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T013 [US1] Wire up `main()`: call parseInput → initSP → pack (with initial square shapes: `w=ceil(sqrt(area))`, `h=ceil(area/w)`) → writeFloorplan; run on `../../benchmark/testcase/sample.txt`; confirm `scorer/score.py` returns `valid: True` in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`

**Checkpoint**: `./hw3 ../../benchmark/testcase/sample.txt out_sample.floorplan` produces a legal floorplan (`valid: True`). US1 is independently complete.

---

## Phase 4: User Story 2 — HPWL Minimization (Priority: P2)

**Goal**: Attach a Simulated Annealing optimizer that minimizes weighted HPWL, targeting values below the Min thresholds (see research.md §1).

**Independent Test**: Run on `public3.txt` (smallest public testcase) → `valid: True` and `wirelength < 2,621,582` (better than Reference).

### Implementation for User Story 2

- [x] T014 [US2] Implement `computeHPWL(const vector<Module>& allMods, const vector<Net>& nets)` → `long long`: for each net (a, b, weight), compute `cx=x+w/2, cy=y+h/2` (integer division), accumulate `weight * (abs(cx_a - cx_b) + abs(cy_a - cy_b))` in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T015 [US2] Implement `computeCost(long long hpwl, int ow, int oh, double lambda)` → `double`: returns `hpwl + lambda * (ow + oh)`; lambda is set to `10 * initial_HPWL` so outline violations are heavily penalized in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T016 [P] [US2] Implement `perturbSwapGP(SequencePair& sp, int n)`: pick two distinct random indices i,j in `[0,n)`, swap `sp.pos_x[i]` and `sp.pos_x[j]`; return undo info `{i,j}` in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T017 [P] [US2] Implement `perturbSwapGM(SequencePair& sp, int n)`: same as T016 but swap in `sp.pos_y` in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T018 [P] [US2] Implement `perturbRotatePair(SequencePair& sp, int n)`: pick a random module index k; move element k in `pos_x` to a random new position; move same element in `pos_y` to a different random position; return undo info in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T019 [P] [US2] Implement `perturbResizeAR(vector<Module>& softMods, int n_soft)`: pick a random soft module i; pick a random (w,h) from `softMods[i].shapes` different from current; update `softMods[i].w` and `.h`; return undo info `{i, old_w, old_h}` in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T020 [US2] Implement `calibrateTemp(SequencePair sp, vector<Module> softMods, const vector<Module>& fixedMods, const vector<Net>& nets, int chip_W, int chip_H, double lambda)` → `double T0`: run 1000 random perturbations (mix of all 4 operators), compute mean `|ΔC|`, return that as T0 (sets initial acceptance prob ≈ 1.0 for average-size moves) in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T021 [US2] Implement the SA main loop in `main()`: while `T > 0.1 && elapsed < 580s`, iterate `N = max(50 * n_soft * n_soft, 10000)` perturbation attempts per temperature; each attempt: pick operator (uniform random), apply perturbation, pack, compute cost, accept if `ΔC < 0` or `rand() < exp(-ΔC/T)`, else undo; track global best (min HPWL with no overflow); cool with `T *= 0.92` in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T022 [US2] Run on `public3.txt`: verify `valid: True` and `wirelength < 2,621,582` (Reference); if HPWL is still above Reference, debug SA cost function or perturbation undo logic in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T023 [US2] Run on `public2.txt`: verify `valid: True` and compare HPWL against Min=20,966,863; record result in `problems/002-floorplanning/experiments/claude-sonnet-4-6/RESULT.md`

**Checkpoint**: public2 and public3 produce valid results with HPWL below Reference values. US2 core optimization is working.

---

## Phase 5: User Story 3 — Runtime Within 600s (Priority: P3)

**Goal**: All 5 testcases complete in ≤ 580s (leaving 20s buffer); largest testcase (public1, 15 soft modules, chip 11267×10450) runs within budget.

**Independent Test**: `time ./hw3 ../../benchmark/testcase/public1.txt out_public1.floorplan` → wall time < 600s and `valid: True`.

### Implementation for User Story 3

- [x] T024 [US3] Replace any `clock()`-based timer with `chrono::steady_clock` for accurate wall-clock measurement; add a `timeUp(start, 580.0)` helper that returns true when 580 seconds have elapsed; use this in the SA loop termination condition in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T025 [US3] Run on `public1.txt` and measure actual runtime; if runtime exceeds 580s, reduce `N_moves` per temperature to `max(20 * n_soft * n_soft, 5000)` or increase cooling rate to `0.90`; ensure the binary terminates within 580s and outputs the best-so-far solution found before the time limit in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T026 [US3] Run full batch on all 5 testcases (sample + public1–4); for each: record `valid`, `HPWL`, `runtime`; verify all finish within 580s; create initial `problems/002-floorplanning/experiments/claude-sonnet-4-6/RESULT.md` with results table

**Checkpoint**: All 5 testcases produce `valid: True` within 580s. US3 runtime requirement met.

---

## Phase 6: Polish & Cross-cutting Concerns

**Purpose**: Finalize output quality, documentation, and compliance with Constitution Principles IV and V.

- [x] T027 Ensure `writeFloorplan()` writes the self-reported `Wirelength` value from the **best** solution (not current SA state) as the first line; confirm format matches [contracts/output-format.md](contracts/output-format.md) exactly in `problems/002-floorplanning/experiments/claude-sonnet-4-6/main.cpp`
- [x] T028 [P] Finalize `problems/002-floorplanning/experiments/claude-sonnet-4-6/RESULT.md` with complete table:

  ```markdown
  | testcase | valid | HPWL | vs Min (%) | vs Reference (%) | runtime (s) |
  ```

  Include rows for sample and public1–4; note model version (`claude-sonnet-4-6`) and compile command
- [x] T029 [P] Validate output files for all testcases against [contracts/output-format.md](contracts/output-format.md): check `Wirelength` line, `NumSoftModules` count, all soft module names present, no fixed module names in output
- [x] T030 Run quickstart.md Scenario 4 (format validation) for all 5 testcases; confirm scorer `valid: True` for all in `problems/002-floorplanning/experiments/claude-sonnet-4-6/`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 — **BLOCKS all user stories**
- **US1 (Phase 3)**: Depends on Phase 2 — core placement
- **US2 (Phase 4)**: Depends on Phase 3 completion (needs working pack + output)
- **US3 (Phase 5)**: Depends on Phase 4 (needs SA loop in place)
- **Polish (Phase 6)**: Depends on Phase 5

### User Story Dependencies

- **US1 (P1)**: Parser + SequencePair packer + output writer — can start immediately after Foundation
- **US2 (P2)**: Requires US1 working packer (pack() used in SA loop)
- **US3 (P3)**: Requires US2 SA loop (adds time limit)

### Within Each Phase — Parallelizable Tasks

- T016, T017, T018, T019 (four SA operators) can be written in parallel — independent functions
- T028, T029 (RESULT.md + format validation) can run in parallel — different outputs

### Critical Path

```
T001 → T002 → T003 → T004 → T005 → T006 → T007 → T008 → T009
→ T010 → T011 → T012 → T013                   ← US1 complete
→ T014 → T015 → [T016‖T017‖T018‖T019] → T020 → T021 → T022 → T023   ← US2 complete
→ T024 → T025 → T026                          ← US3 complete
→ [T027 → T028‖T029] → T030                  ← Polish complete
```

---

## Parallel Execution Examples

```bash
# US2 SA operator tasks — all independent functions, run in parallel:
Task T016: "Implement perturbSwapGP operator"
Task T017: "Implement perturbSwapGM operator"
Task T018: "Implement perturbRotatePair operator"
Task T019: "Implement perturbResizeAR operator"

# Polish phase — run in parallel after US3:
Task T028: "Finalize RESULT.md"
Task T029: "Validate output format"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1 + Phase 2 (T001–T005)
2. Complete Phase 3 US1 (T006–T013)
3. **STOP and VALIDATE**: `./hw3 sample.txt out.floorplan` → `valid: True`
4. This gives a working (but unoptimized) floorplanner

### Incremental Delivery

1. **After US1** (T013): Produces legal floorplans — verifier passes, HPWL unoptimized
2. **After US2** (T023): Optimizes HPWL — should beat Reference values
3. **After US3** (T026): All testcases finish within 580s
4. **After Polish** (T030): RESULT.md ready for cross-model comparison

### Single-file C++ Note

All implementation lives in `main.cpp`. Functions should be defined in dependency order (called functions before callers), or use forward declarations. Suggested order in file:

```
1. Structs: Module, Net, SequencePair
2. genShapes()
3. parseInput()
4. longestPath()
5. buildHCG(), buildVCG()
6. pack()
7. computeHPWL(), computeCost()
8. perturbSwapGP(), perturbSwapGM(), perturbRotatePair(), perturbResizeAR()
9. calibrateTemp()
10. writeFloorplan()
11. main()
```

---

## Notes

- `[P]` tasks have no intra-task dependencies and can be implemented by parallel agents
- Each checkpoint validates the story independently via `scorer/score.py`
- If HPWL targets (Min) are not reached after US2, document the gap in RESULT.md per Constitution Principle VI ("若最終仍無法超越 baseline，須在 RESULT.md 記錄與 baseline 的差距及原因分析")
- Fixed modules are NOT written to output; only soft module positions appear in `.floorplan`
- Center formula for HPWL: `cx = x + w/2` using **integer** (floor) division — must match `scorer/lib/floorplanning.py` exactly
