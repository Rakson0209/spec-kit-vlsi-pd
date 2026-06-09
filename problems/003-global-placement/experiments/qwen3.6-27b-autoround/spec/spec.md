# Feature Specification: Global Placement (HPWL Minimization)

**Feature Branch**: `003-global-placement`

**Created**: 2026-06-08

**Status**: Draft

**Input**: User description: "全域佈局 (Global Placement)。完整題目見 problems/003-global-placement/reference/spec.pdf。輸入為 Bookshelf 格式:一個 .aux 指向 .nodes(cell 與尺寸)/.nets(net 連接)/.pl(初始座標)/.scl(row/site)/.wts(net 權重)。需求:求每個 cell 的座標使半周長線長 HPWL 最小化。輸出 .gp.pl 為各 cell 最終座標。測資在 benchmark/testcase/public1~3/。合法性與計分一律用 scorer/(純 Python,Windows 可跑)。驗收:先確保全部 testcase 經 scorer 合法(模組在 core 內、固定模組未移動),再追求 HPWL ≤ 各 testcase 的 Min(見本題 README 的 Baseline 指標門檻表)。"

## Overview

The deliverable is a command-line tool that reads one **Bookshelf** circuit (a `.aux` file pointing at `.nodes`, `.nets`, `.pl`, `.scl`, `.wts`) and writes one `.gp.pl` placement-result file giving the final coordinate of every movable cell. The objective is to place cells so as to **minimize total half-perimeter wirelength (HPWL)** over the net list.

This is **global placement**: cells are allowed to overlap, and HPWL is scored on that overlapping ("global") placement. There are two kinds of cells:

- **Movable cells** — standard cells whose lower-left coordinate the tool chooses freely, subject only to staying fully inside the core region.
- **Fixed cells** — terminals (declared `terminal` in `.nodes`) and any cell marked `FIXED` in `.pl`. Their positions are pinned by the input; the tool must never move them, and they act as fixed anchors for the wirelength objective.

A naive reading — "let cells overlap, so just collapse every cell onto its highest-degree neighbor for near-zero HPWL" — is explicitly **forbidden by the scorer**. Although HPWL is measured on the overlapping placement, the scorer also runs a row-based Tetris **legalizability health check**: it spreads the placement onto the `.scl` rows/sites and measures the average displacement required. A genuinely spread-out placement legalizes with tiny displacement; a collapsed placement requires huge displacement and is rejected as illegal. The tool must therefore produce a placement that is both **low-HPWL and spreadable** (cells distributed across the core with low local density), not a degenerate pile.

This feature exists to **compare how well different models implement the same specification**. Acceptance is judged exclusively by the project scorer (`scorer/lib/placement.py`, `scorer/lib/bookshelf.py`, `scorer/lib/legalize.py`); the scorer is the single source of truth for both legality and the HPWL metric. Where the full problem statement (`reference/spec.pdf`) and the scorer diverge, **the scorer wins**.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Produce a legal placement for every testcase (Priority: P1)

The operator runs the tool on a testcase `.aux` and gets back a `.gp.pl` that the scorer accepts as **valid**: every movable cell has an output coordinate, every movable cell lies fully inside the core, no fixed cell or terminal is moved, and the placement passes the legalizability health check (it is spread out, not collapsed). This is the non-negotiable gate — an output that fails legality scores nothing, regardless of its HPWL.

**Why this priority**: Legality is the hard pass/fail threshold. A placement with tiny HPWL but a missing cell, an out-of-core cell, a moved terminal, or a collapsed pile delivers zero value. This is the minimum viable product.

**Independent Test**: Run the tool on each of the 3 testcases (`public1`–`public3`), then run `python scorer/score.py 003 --output-dir <out>`; every case must report `valid=OK` (zero violations), including a `legalize` note whose average displacement is at or below `0.05×` the core's shorter side.

**Acceptance Scenarios**:

1. **Given** a public testcase `.aux`, **When** the tool runs with an input `.aux` path and an output `.gp.pl` path, **Then** it writes a coordinate for every movable cell, each fully inside the core, with no terminal/fixed cell relocated, so the scorer reports `valid=OK`.
2. **Given** the scorer's legalizability health check, **When** the model's placement is spread onto the `.scl` rows via single-row Tetris, **Then** the average displacement is `≤ 0.05 × min(coreW, coreH)` and the legalize step does **not** abort (it never reaches the `0.15×` collapse threshold).
3. **Given** `public2` (which contains 1201 terminals / fixed cells), **When** the tool runs, **Then** every terminal's output coordinate is identical to its `.pl` input coordinate (within `1e-6`), and only movable cells are repositioned.

---

### User Story 2 - Minimize HPWL to beat the baseline targets (Priority: P2)

Given a legal placement, the operator wants the **smallest possible HPWL**. Each public testcase has a published **Min** target (the value to beat, lower is better), a **Reference** value (the human baseline), and a **Max** zero-score threshold. The tool should drive HPWL down to at or below the Min target, and at minimum stay strictly below Max so the result scores at all.

**Why this priority**: HPWL is the optimization metric and the headline comparison between models. It only matters once legality (P1) holds — and because of the health check, lowering HPWL must be achieved by genuinely shortening connections, not by piling cells up.

**Independent Test**: Score each public testcase and compare the recomputed `hpwl` against the per-case Min / Reference / Max table; verify each case is strictly below Max, and measure how many reach ≤ Reference and ≤ Min.

**Acceptance Scenarios**:

1. **Given** a legal placement for a public testcase, **When** the scorer recomputes HPWL on the overlapping global placement, **Then** the value is strictly below that case's **Max** (zero-score) threshold.
2. **Given** a legal placement, **When** HPWL is measured on every public case, **Then** it is at or below that case's **Reference** value on every case, and strictly below it on at least one case (so a self-written solution is competitive with the human baseline).
3. **Given** a legal placement, **When** HPWL is measured, **Then** it meets the headline goal of ≤ the case's **Min** target.

---

### User Story 3 - Converge within the runtime budget at scale (Priority: P3)

The operator runs the tool on every public case and it finishes within the runtime budget while still producing a legal, low-HPWL placement. The difficulty is **scale**: the largest case has ~51k cells, ~50k nets, and ~188k pins. The placement search (analytical/force-directed spreading, or partitioning-based) must converge within the budget on these sizes.

**Why this priority**: Correctness and quality (P1/P2) are only achievable if the optimization actually finishes. A search that would eventually find a great placement but exceeds the budget produces no usable result. The largest case dominates the comparison.

**Independent Test**: Run on `public3` (largest: 51,382 cells / 50,393 nets / 246 rows) and confirm it completes within the runtime budget and the scorer reports `valid=OK` with a finite HPWL.

**Acceptance Scenarios**:

1. **Given** the largest testcase (`public3`), **When** the tool runs, **Then** it completes within the runtime budget (~590 seconds) and exits with status 0.
2. **Given** nets connecting movable cells to **fixed** terminals (as in `public2`), **When** the tool places movable cells, **Then** those cells are pulled toward the terminals and other neighbors they connect to, reducing HPWL rather than being placed arbitrarily.

---

### Edge Cases

- **Overlap is allowed, collapse is not**: cells may overlap in the scored global placement, so HPWL is measured with overlapping coordinates — but the legalizability health check rejects any placement that cannot be spread onto the rows within `0.05 × min(coreW, coreH)` average displacement. The tool must keep local density low enough to legalize cheaply.
- **Single-row cell height**: every movable cell's height equals the row height (single-row cells), so legalization reduces to choosing a row and an in-row position. The tool should produce coordinates consistent with this row structure (cells aligned near valid rows lower the legalize displacement and HPWL gap).
- **Core boundary is inclusive at the lower-left, exclusive of overflow**: a movable cell at lower-left `(x, y)` with size `(w, h)` is in-core iff `x ≥ xmin`, `y ≥ ymin`, `x + w ≤ xmax`, `y + h ≤ ymax` (within `1e-6`), where the core bounds come from the `.scl` rows.
- **Negative coordinates are normal**: core origin can be negative (e.g., `public1` rows start near `-33330`); the tool must handle negative `Coordinate` / `SubrowOrigin` values and not assume a `(0,0)` origin.
- **Pin offsets relative to lower-left**: a pin's global coordinate is `(cell.x + xoff, cell.y + yoff)`, where `xoff/yoff` are read from the `.nets` pin lines relative to the cell's lower-left corner. The tool's internal HPWL estimate must use the same convention so its self-reported value matches the scorer's recomputation.
- **Net weights are not applied by the scored metric**: the scorer computes the **unweighted** sum of per-net half-perimeters; it does not multiply by `.wts` values. The scored objective is therefore unweighted HPWL. The tool may read `.wts` to guide its search, but the value it is judged on ignores weights.
- **Fixed cells span both files**: a cell is fixed if it is a `terminal` in `.nodes` **or** marked `FIXED` in `.pl`. Both must be treated as immovable anchors.
- **Output need only cover movable cells**: the scorer overlays the model's output onto the original `.pl`; movable cells missing from the output are flagged as violations, while fixed cells default to their `.pl` coordinate. Emitting fixed cells is allowed only if their coordinates exactly match the input.
- **Whitespace, comments, UCLA headers**: all Bookshelf files contain `# ...` comments, blank lines, and a `UCLA ...` header line; the parser must skip these and tokenize on whitespace.
- **Degenerate / self-referencing nets**: a net may have degree 1 or repeat a node; such a net contributes a well-defined (possibly zero) half-perimeter and must not crash the tool.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The tool MUST accept exactly two command-line arguments — an input `.aux` path and an output `.gp.pl` path — and be invocable as `tool <input.aux> <output.gp.pl>` (matching how the scorer launches it).
- **FR-002**: The tool MUST parse the Bookshelf input set reached from the `.aux`: `.nodes` (`<name> <w> <h> [terminal]`), `.nets` (`NetDegree : <k>` blocks of `<node> <I/O> : <xoff> <yoff>` pin lines), `.pl` (`<name> <x> <y> : <orient> [/FIXED]`, lower-left coordinates), `.scl` (`CoreRow` blocks giving row `Coordinate`, `Height`, `Sitewidth`/`Sitespacing`, `SubrowOrigin`, `NumSites`), and `.wts` (`<net-or-node> <weight>`). Parsing MUST skip `#` comments, `UCLA` headers, and blank lines, and tokenize on whitespace.
- **FR-003**: The tool MUST classify each cell as movable or fixed: a cell is **fixed** iff it is declared `terminal` in `.nodes` or marked `FIXED` in `.pl`; otherwise it is movable. The tool MUST assign a final coordinate to every movable cell and output every movable cell exactly once (full coverage — none missing, none duplicated).
- **FR-004**: Every movable cell MUST lie fully inside the core: with lower-left `(x, y)` and size `(w, h)`, `x ≥ xmin`, `y ≥ ymin`, `x + w ≤ xmax`, `y + h ≤ ymax`, where `(xmin, ymin, xmax, ymax)` is the core derived from the `.scl` rows (evaluated within `1e-6`).
- **FR-005**: The tool MUST NOT move any fixed cell or terminal: every fixed cell's effective final coordinate MUST equal its `.pl` input coordinate (within `1e-6`). If the tool writes fixed cells to the output, it MUST write them at exactly their input coordinates.
- **FR-006**: The tool's output placement MUST pass the scorer's legalizability health check: when spread onto the `.scl` rows by single-row Tetris (`scorer/lib/legalize.py`), the average displacement MUST be `≤ 0.05 × min(coreW, coreH)` and the legalize step MUST NOT abort (it must stay below the `0.15×` collapse-abort threshold). Equivalently, the placement MUST be genuinely spread across the core, not collapsed.
- **FR-007**: The tool MUST minimize total HPWL, defined as `Σ_nets [ (max_pins x − min_pins x) + (max_pins y − min_pins y) ]`, where each pin's global coordinate is `(cell.x + xoff, cell.y + yoff)`. This is the unweighted sum exactly as `scorer/lib/bookshelf.py:compute_hpwl` computes it.
- **FR-008**: The tool MUST write the output in Bookshelf placement (`.gp.pl`) format: one line per emitted cell of the form `<name> <x> <y> : <orient>` (orientation may be `N`), parseable by `scorer/lib/bookshelf.py:parse_pl`. The output MUST include every movable cell.
- **FR-009**: The tool's output MUST be judged **valid** by the scorer for all 3 provided testcases (`public1`–`public3`).
- **FR-010**: For every public testcase, the scorer's recomputed HPWL MUST be strictly below that case's **Max** zero-score threshold.
- **FR-011**: The tool MUST finish each testcase within the runtime budget of approximately **590 seconds** and exit with status code 0; the scorer treats any non-zero exit as a failed run.
- **FR-012**: The tool MUST handle the full range of testcase scales (up to ~51k cells, ~50k nets, ~188k pins, 246 rows) and negative core coordinates without crashing or producing out-of-core coordinates.
- **FR-013**: The tool MUST be deterministic enough that re-running on the same input yields a placement of equal-or-better HPWL and identical legality (no flaky legality failures across runs).

### Key Entities

- **Core region**: the placement area, derived from the `.scl` `CoreRow` blocks as `(xmin, ymin, xmax, ymax)` = (min `SubrowOrigin`, min row `Coordinate`, max `SubrowOrigin + NumSites × Sitespacing`, max `Coordinate + Height`). May have negative origin. Every movable cell must lie within it.
- **Row / site**: a horizontal placement track at vertical `Coordinate` with a given `Height`, spanning `[SubrowOrigin, SubrowOrigin + NumSites × Sitespacing]`, gridded into sites of width `Sitespacing`. Movable cells are single-row-height and ultimately occupy rows; row structure drives the legalizability health check.
- **Movable cell**: a standard cell identified by name with size `(w, h)` from `.nodes`; the tool chooses its lower-left `(x, y)`. Contributes pins to the nets it appears in.
- **Fixed cell / terminal**: a cell whose coordinate is pinned by the input (`terminal` in `.nodes` or `FIXED` in `.pl`). It is an immovable anchor for HPWL and never repositioned.
- **Net**: a connection over a set of cells; each membership carries a pin offset `(xoff, yoff)` relative to the cell's lower-left. Contributes `(max pin_x − min pin_x) + (max pin_y − min pin_y)` to HPWL.
- **Pin**: a connection point on a cell at global coordinate `(cell.x + xoff, cell.y + yoff)`.
- **Net weight**: a per-net value in `.wts`. Present in the input but **not** applied by the scored HPWL metric; usable only as an internal search hint.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001 (mandatory legality gate)**: All 3 testcases (`public1`–`public3`) are reported `valid=OK` by `scorer/lib/placement.py` — zero completeness, out-of-core, moved-fixed, and legalizability-health (anti-collapse) violations.
- **SC-002 (mandatory scoring gate)**: For every public testcase, the scorer's recomputed HPWL is strictly below that case's **Max** zero-score threshold.
- **SC-003 (headline target)**: For every public testcase, HPWL is at or below the case's **Min** target:

  | testcase | Min (target, ≤) | Reference (baseline) | Reference runtime | Max (zero-score) |
  |----------|----------------:|---------------------:|------------------:|-----------------:|
  | public1  |      59,788,412 |           87,987,694 |            28.43s |      319,198,465 |
  | public2  |      10,530,075 |           18,642,174 |            58.95s |       28,999,635 |
  | public3  |     395,131,978 |          750,902,922 |           110.7s  |    2,631,834,205 |

- **SC-004 (competitive with baseline)**: HPWL is at or below the **Reference** value on every public case, and strictly below it on at least one case, so a self-written solution is competitive with the human baseline (per constitution R1, the self-written code is kept as long as any case beats Reference).
- **SC-005 (runtime)**: Each public case completes within ~590 seconds wall-clock and the tool exits 0.
- **SC-006 (spreadability)**: On every public case the scorer's reported legalize average displacement is `≤ 0.05 × min(coreW, coreH)` (the placement legalizes cheaply, confirming it is not collapsed).

## Assumptions

- **Scorer is the source of truth** (constitution R6): legality and HPWL are whatever `scorer/lib/{placement,bookshelf,legalize}.py` compute. The course's official Linux `computeHpwl` is not used for grading here; the pure-Python scorer is, so its conventions (lower-left coordinates, pin offsets relative to lower-left, unweighted per-net half-perimeter, anti-collapse health check) govern.
- **Movable cells are single-row height**: confirmed for all three public cases by the scorer's legalizer; the tool may rely on each movable cell occupying exactly one row.
- **HPWL is scored on the overlapping global placement**, not on a legalized placement; the legalized placement is used only for the health check and is reported in the `note`, not as the scored metric.
- **Net weights are ignored by the scored metric**; the Min/Reference/Max thresholds correspond to unweighted HPWL.
- **Runtime budget is ~590 seconds** per case (from the README), interpreted as a hard wall-clock ceiling; the reference solutions finish far faster (28–111s), leaving headroom for a stronger search.
- **Output orientation** may be written as `N` for all movable cells; the scorer reads only the name and coordinates from `.gp.pl`.
- **Environment**: built with the project's portable toolchain (`g++ -std=c++20 -O3 -fopenmp -pthread`, per constitution R2) and run on Windows; scoring is pure Python and cross-platform.
