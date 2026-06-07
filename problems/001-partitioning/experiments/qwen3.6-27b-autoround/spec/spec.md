# Feature Specification: Multi-Technology Die Partitioning

**Feature Branch**: `001-die-partitioning`

**Created**: 2026-06-07

**Status**: Draft

**Input**: User description: "多技術晶粒切割 (Die Partitioning)。把每個 cell 指派到 DieA 或 DieB，滿足兩 die 的面積使用率上限，最小化跨 die 的切割 cut size。輸入 .txt（NumTechs/Tech/LibCell、DieSize、DieA/DieB、NumCells/Cell、NumNets/Net），輸出 .out 為每個 cell 的 die 指派。測資 sample + public1~6，合法性與計分用 scorer/（純 Python）。驗收：先全部 testcase 經 scorer 合法，再追求 cut size ≤ 各 testcase 的 Min。"

## Overview

The deliverable is a command-line tool that reads one circuit description file and writes one partition-result file. Each circuit's cells must be split across **two dies (DieA, DieB)** that use **different process technologies**. Because the same library cell occupies a different area under each technology, the same cell consumes a different fraction of each die. The tool must produce a **legal** partition (every die stays within its area-utilization cap) while **minimizing the cut size** — the number of nets whose cells end up on both dies.

This feature exists to **compare how well different models implement the same specification**. Acceptance is judged exclusively by the project scorer (`scorer/lib/partitioning.py`); the scorer is the single source of truth for both legality and the cut-size metric.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Produce a legal partition for every testcase (Priority: P1)

The operator runs the tool on a testcase input file and gets back an output file that the scorer accepts as **valid**: every cell is assigned to exactly one die, and neither die exceeds its area-utilization cap. This is the non-negotiable gate — an output that fails legality scores nothing, regardless of cut size.

**Why this priority**: Legality is the hard pass/fail threshold. A tool that produces low cut sizes but illegal partitions delivers zero value. This is the minimum viable product.

**Independent Test**: Run the tool on each of the 7 testcases (`sample`, `public1`–`public6`), then run `python scorer/score.py 001 --output-dir <out>`; every case must report `valid=OK` (zero violations).

**Acceptance Scenarios**:

1. **Given** the `sample` input (8 cells, 6 nets, 2 techs), **When** the tool runs with an input path and an output path, **Then** it writes an output file in which all 8 cells appear exactly once across DieA/DieB and both dies are within their utilization caps, and the scorer reports `valid=OK`.
2. **Given** any `public` testcase, **When** the tool runs, **Then** the scorer reports no coverage violations (no missing, unknown, or duplicated cells) and no utilization violations for either die.
3. **Given** a circuit where one die's cap is tighter than the other, **When** the tool assigns cells, **Then** the area of each die — computed with that die's own technology dimensions — stays at or below its cap (within the scorer's `1e-9` tolerance).

---

### User Story 2 - Minimize cut size to beat the baseline targets (Priority: P2)

Given a legal partition, the operator wants the **smallest possible cut size**. Each testcase has a published **Min** target (the value to beat, lower is better), a **Reference** value (the human baseline), and a **Max** zero-score threshold. The tool should drive cut size down to at or below the Min target, and at minimum stay below Max so the result scores at all.

**Why this priority**: Cut size is the optimization metric and the headline comparison between models. It only matters once legality (P1) holds.

**Independent Test**: Score each testcase and compare the recomputed `cut_size` against the per-case Min / Reference / Max table; verify each case is below Max, and measure how many reach ≤ Reference and ≤ Min.

**Acceptance Scenarios**:

1. **Given** a legal partition for a public testcase, **When** the scorer recomputes cut size, **Then** the value is strictly below that case's Max (zero-score) threshold.
2. **Given** a legal partition, **When** cut size is measured, **Then** it is at or below that case's Reference value on every case and strictly below it on at least one case (so a self-written solution is competitive with the human baseline).
3. **Given** a legal partition, **When** cut size is measured, **Then** it meets the headline goal of ≤ the case's Min target.

---

### User Story 3 - Scale to the largest testcases within the runtime budget (Priority: P3)

The operator runs the tool on the largest inputs (millions of lines, tens of megabytes) and the tool finishes within the runtime budget without running out of memory or crashing, still producing a legal, low-cut output.

**Why this priority**: Correctness and quality (P1/P2) are meaningless on the big cases if the tool cannot parse them or does not finish in time. The largest case dominates the comparison.

**Independent Test**: Run on `public6` (~4.15M lines) and `public3`/`public5` (multi-MB); confirm each completes within the runtime budget and the scorer reports `valid=OK` with a finite cut size.

**Acceptance Scenarios**:

1. **Given** the largest testcase, **When** the tool runs, **Then** it completes within the runtime budget (~300 seconds) and exits with status 0.
2. **Given** a multi-megabyte input with blank lines between sections and varied whitespace, **When** the tool parses it, **Then** parsing succeeds and all sections (techs, die definitions, cells, nets) are read correctly.

---

### Edge Cases

- **Per-die area asymmetry**: a cell's area differs between DieA's and DieB's technology, so the same cell may fit one die's remaining capacity but not the other's; the chosen side must respect the *destination* die's cap.
- **Degree-1 nets**: a net with a single cell can never be cut and never contributes to cut size.
- **Already-uncut nets**: a net whose cells are all on one die contributes 0; moving any one member to the other die newly cuts it.
- **Repeated membership within a net**: cut is determined by which *sides* a net touches, so a cell listed more than once in one net does not change that net's cut contribution.
- **Tight caps / feasibility**: caps may be tight enough that finding *any* legal split is non-trivial; the tool must still produce a legal assignment.
- **Skewed-but-legal splits**: there is no minimum-occupancy or balance-ratio rule — placing all cells on one die is legal *iff* that die's cap holds (cut size would then be 0). Balance is enforced only by the two caps jointly.
- **Self-reported vs recomputed cut size**: the output may state a `CutSize`, but the scorer recomputes it; a stated value that disagrees with the actual partition is misleading and must be avoided.
- **Whitespace / blank lines**: inputs contain blank lines between sections and may use arbitrary spacing; the parser must skip blank lines and tokenize on whitespace.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The tool MUST accept exactly two command-line arguments — an input file path and an output file path — and be invocable as `tool <input.txt> <output.out>` (matching how the scorer launches it).
- **FR-002**: The tool MUST parse the input grammar: `NumTechs`; per technology a `Tech <name> <numLibCells>` block followed by its `LibCell <name> <width> <height>` lines; `DieSize <W> <H>`; `DieA <techName> <util%>` and `DieB <techName> <util%>`; `NumCells` followed by `Cell <cellName> <libCellName>` lines; `NumNets` followed by `Net <netName> <degree>` blocks each listing `Cell <cellName>` members. Parsing MUST tolerate blank lines between sections and arbitrary whitespace.
- **FR-003**: The tool MUST assign every cell to exactly one die — no cell left unassigned, none duplicated across or within dies, and none referencing an unknown cell (full, exclusive coverage).
- **FR-004**: For each die, the tool MUST keep that die's used area within its utilization cap, where used area = sum over the die's cells of the cell's area, and the cap test is `usedArea / (DieW × DieH) ≤ util% / 100` (≤, satisfied within a `1e-9` tolerance).
- **FR-005**: The tool MUST compute a cell's area using the dimensions of its library cell **under the technology assigned to the die it is placed on**; the same library cell has different dimensions per technology, so a cell's contributed area depends on its die.
- **FR-006**: The tool MUST write the output file in the required format: an optional first line `CutSize <n>`, then a line `DieA <countA>` followed by `countA` lines each naming a cell assigned to DieA, then a line `DieB <countB>` followed by `countB` lines each naming a cell assigned to DieB. The cell name MUST be the first token on each cell line.
- **FR-007**: When the tool emits the optional `CutSize <n>` line, the stated value MUST equal the cut size actually implied by the written partition.
- **FR-008**: The tool MUST minimize cut size, defined as the number of nets that have at least one cell on DieA and at least one cell on DieB.
- **FR-009**: The tool's output MUST be judged **valid** by the scorer for all 7 provided testcases (`sample`, `public1`–`public6`).
- **FR-010**: The tool MUST finish each testcase within the runtime budget of approximately 300 seconds.
- **FR-011**: The tool MUST handle the largest provided inputs (up to ~4 million lines / tens of megabytes) without crashing or exhausting memory.
- **FR-012**: The tool MUST exit with status code 0 on success; the scorer treats any non-zero exit as a failed run.
- **FR-013**: The tool MUST be deterministic enough that re-running on the same input yields a partition of equal-or-better cut size and identical legality (no flaky failures across runs).

### Key Entities

- **Technology (Tech)**: a named process. Defines, for each library cell, a width and height (and thus an area). Two technologies are present; DieA and DieB each bind to one of them.
- **LibCell**: a library cell type identified by name. Its width/height — and therefore its area — differ per technology.
- **Die (DieA / DieB)**: a physical die bound to exactly one technology, with a fixed area equal to `DieSize` (`W × H`) and a maximum utilization cap (a percentage). Holds a subset of the circuit's cells.
- **Cell**: an instance referencing one LibCell. Must be assigned to exactly one die; the area it consumes depends on its die's technology.
- **Net**: a hyperedge connecting a set of cells (its degree is the member count). A net is "cut" — contributing 1 to cut size — exactly when its members span both dies.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001 (mandatory legality gate)**: All 7 testcases (`sample`, `public1`–`public6`) are reported `valid=OK` by `scorer/lib/partitioning.py` (zero coverage and zero utilization violations).
- **SC-002 (mandatory scoring gate)**: For every public testcase, the scorer's recomputed cut size is strictly below that case's **Max** zero-score threshold.
- **SC-003 (headline target)**: For every public testcase, cut size is at or below the case's **Min** target:

  | testcase | Min (target, ≤) | Reference (baseline) | Reference runtime | Max (zero-score) |
  |----------|----------------:|---------------------:|------------------:|-----------------:|
  | public1  | 104             | 193                  | 0.03s             | 1,441            |
  | public2  | 816             | 3,666                | 1.59s             | 27,862           |
  | public3  | 1,762           | 7,092                | 47.95s            | 103,659          |
  | public4  | 982             | 2,265                | 1.09s             | 12,421           |
  | public5  | 297             | 1,669                | 15.4s             | 48,964           |
  | public6  | 5,159           | 10,281               | 225.47s           | 490,120          |

- **SC-004 (baseline-competitiveness gate)**: Cut size is at or below the **Reference** value on every public case and strictly below it on at least one case — demonstrating the solution is at least competitive with the human baseline.
- **SC-005 (runtime)**: Each testcase completes within ~300 seconds.
- **SC-006 (self-report consistency)**: Wherever the output states a `CutSize`, it equals the scorer-recomputed cut size for that case.

## Assumptions

- **Scorer is authoritative**: `scorer/lib/partitioning.py` is the sole authority for legality and the cut-size metric. The full problem statement (`reference/spec.pdf`) is assumed consistent with it; where it diverges, the scorer wins.
- **"Balance constraint" = the two utilization caps**: the user's mention of a balance limit is realized solely through the per-die utilization upper bounds. The scorer enforces no separate balance-ratio, minimum-occupancy, or maximum-cell-count rule. A single-die solution is legal iff that die's cap holds.
- **Utilization basis**: each die's cap is a fraction of the full die area `DieSize.W × DieSize.H`; both dies share the same physical area. The comparison is `≤` with a `1e-9` tolerance.
- **LibCell coverage**: every LibCell referenced by a cell is defined under both technologies (as in `sample`); cells whose LibCell is missing from a die's technology would contribute 0 area there per the scorer, a situation assumed not to occur in the provided testcases.
- **Output ordering is free**: the order of cell names within the DieA / DieB lists does not affect legality or cut size; the scorer only checks set membership and counts.
- **Invocation**: the scorer launches the tool as `[exe, input_path, output_path]` and matches output files by case name with extension `.out`.
- **Input well-formedness**: inputs follow the documented grammar; the tool need not defend against adversarially malformed inputs beyond skipping blank lines and tolerating whitespace.
- **Runtime budget**: ~300 seconds per testcase on a standard multi-core machine (the human reference reached ~225s on the largest case).
