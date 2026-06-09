# Feature Specification: Fixed-Outline Floorplanning

**Feature Branch**: `002-fixed-outline-floorplanning`

**Created**: 2026-06-08

**Status**: Draft

**Input**: User description: "固定輪廓平面規劃 (Fixed-outline Floorplanning)。輸入 .txt 含 ChipSize W H、NumSoftModules 與 SoftModule(面積給定、長寬可變形)、NumFixedModules 與 FixedModule(位置尺寸固定)、NumNets 與 Net(A B weight)。需求:所有模組不重疊且完全落在固定晶片輪廓內,最小化加權線長 HPWL。輸出 .floorplan 為各模組最終座標與尺寸。測資在 benchmark/testcase/(sample + public1~4)。合法性與計分一律用 scorer/(純 Python,Windows 可跑)。驗收:先確保全部 testcase 經 scorer 合法(不重疊、在輪廓內、面積/長寬比符合),再追求 wirelength ≤ 各 testcase 的 Min。"

## Overview

The deliverable is a command-line tool that reads one floorplanning description file and writes one floorplan-result file. The chip has a **fixed outline** of width `W` and height `H`. Inside that outline sit two kinds of blocks:

- **Soft modules** — each has a required **minimum area** but a **free shape**: the tool chooses integer width and height, subject to an aspect-ratio window.
- **Fixed modules** — position and size are pinned by the input; the tool must place soft modules *around* them but never move or resize them.

The tool must choose an integer position and shape for every soft module so that (a) every module — soft and fixed — lies fully inside the outline, (b) no two modules overlap, and (c) each soft module honors its minimum-area and aspect-ratio limits, while **minimizing the weighted half-perimeter wirelength (HPWL)** over the net list, where each module contributes a single pin at its geometric center.

This feature exists to **compare how well different models implement the same specification**. Acceptance is judged exclusively by the project scorer (`scorer/lib/floorplanning.py`); the scorer is the single source of truth for both legality and the wirelength metric. Where the full problem statement (`reference/spec.pdf`) and the scorer diverge, the scorer wins.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Produce a legal floorplan for every testcase (Priority: P1)

The operator runs the tool on a testcase input file and gets back an output file that the scorer accepts as **valid**: every soft module is shaped and placed inside the fixed outline, no module overlaps any other module (soft *or* fixed), and every soft module satisfies its minimum-area and aspect-ratio limits. This is the non-negotiable gate — an output that fails legality scores nothing, regardless of its wirelength.

**Why this priority**: Legality is the hard pass/fail threshold. A floorplan with tiny wirelength but an overlap, an out-of-outline block, or an illegal shape delivers zero value. This is the minimum viable product.

**Independent Test**: Run the tool on each of the 5 testcases (`sample`, `public1`–`public4`), then run `python scorer/score.py 002 --output-dir <out>`; every case must report `valid=OK` (zero violations).

**Acceptance Scenarios**:

1. **Given** the `sample` input (chip `8×7`; soft modules `GPU` area 25 and `CPU` area 15; fixed modules `PAD1` at `(0,5,2,2)` and `FIXED1` at `(5,0,3,2)`; 3 nets), **When** the tool runs with an input path and an output path, **Then** it writes an output file that lists both `GPU` and `CPU` exactly once, each fully inside the `8×7` outline, each with `w·h ≥` its minimum area and `0.5 ≤ h/w ≤ 2`, and neither overlapping the other nor `PAD1`/`FIXED1`, so the scorer reports `valid=OK`.
2. **Given** any `public` testcase, **When** the tool runs, **Then** the scorer reports no completeness violations (every soft module present), no out-of-outline violations, no shape violations (area and aspect ratio), and zero overlapping pairs across the combined set of soft and fixed modules.
3. **Given** a soft module declared with minimum area `A`, **When** the tool fixes its shape, **Then** the chosen integer width `w` and height `h` satisfy `w·h ≥ A` and `0.5 ≤ h/w ≤ 2.0` (within the scorer's `1e-9` tolerance).

---

### User Story 2 - Minimize weighted HPWL to beat the baseline targets (Priority: P2)

Given a legal floorplan, the operator wants the **smallest possible weighted HPWL**. Each public testcase has a published **Min** target (the value to beat, lower is better), a **Reference** value (the human baseline), and a **Max** zero-score threshold. The tool should drive wirelength down to at or below the Min target, and at minimum stay below Max so the result scores at all.

**Why this priority**: Wirelength is the optimization metric and the headline comparison between models. It only matters once legality (P1) holds.

**Independent Test**: Score each public testcase and compare the recomputed `wirelength` against the per-case Min / Reference / Max table; verify each case is below Max, and measure how many reach ≤ Reference and ≤ Min.

**Acceptance Scenarios**:

1. **Given** a legal floorplan for a public testcase, **When** the scorer recomputes weighted HPWL, **Then** the value is strictly below that case's **Max** (zero-score) threshold.
2. **Given** a legal floorplan, **When** wirelength is measured on every public case, **Then** it is at or below that case's **Reference** value on every case, and strictly below it on at least one case (so a self-written solution is competitive with the human baseline).
3. **Given** a legal floorplan, **When** wirelength is measured, **Then** it meets the headline goal of ≤ the case's **Min** target.

---

### User Story 3 - Converge to a strong packing within the runtime budget (Priority: P3)

The operator runs the tool on every public case and it finishes within the runtime budget while still producing a legal, low-wirelength floorplan. The difficulty here is the **search**, not the input size: module counts are small (tens of modules), but jointly choosing each soft module's shape *and* a non-overlapping position inside a fixed outline so as to minimize wirelength is a hard combinatorial-geometric optimization — the human reference spends up to ~581 seconds on the largest-outline case.

**Why this priority**: Correctness and quality (P1/P2) are only achievable if the optimization actually finishes. A search that would eventually find a great packing but exceeds the budget produces no usable result. The hardest case dominates the comparison.

**Independent Test**: Run on `public1` (largest outline, `11267×10450`, reference runtime ~581s) and `public3` (most modules and nets: 28 soft / 14 fixed / 108 nets); confirm each completes within the runtime budget and the scorer reports `valid=OK` with a finite wirelength.

**Acceptance Scenarios**:

1. **Given** the largest-outline testcase (`public1`), **When** the tool runs, **Then** it completes within the runtime budget (~600 seconds) and exits with status 0.
2. **Given** nets that connect soft modules to **fixed** modules (e.g., `sample`'s `Net GPU PAD1` and `Net CPU FIXED1`), **When** the tool places soft modules, **Then** soft modules are pulled toward the fixed modules and other neighbors they connect to, reducing the weighted HPWL rather than being placed arbitrarily.

---

### Edge Cases

- **Soft-module shaping with integers**: width and height are integers; the tool must pick a pair with `w·h ≥ minArea` *and* `0.5 ≤ h/w ≤ 2`. A near-square shape (e.g., `w = h = ⌈√A⌉`) always satisfies both, so a legal shape always exists per module — the difficulty is packing every shaped module without overlap inside the outline.
- **Area is a lower bound, not a target**: making a soft module larger than its minimum area is legal but wastes outline space and never improves wirelength (only the center matters), so over-sizing is generally counterproductive.
- **Fixed modules are obstacles *and* net endpoints**: fixed modules occupy outline area that soft modules must avoid, and they also appear in nets (e.g., `sample`), so their fixed centers anchor the wirelength objective. The tool reads their positions from the input and must not emit them in the output.
- **Edge-touching is legal**: overlap is tested with strict inequalities, so modules whose edges coincide (flush against each other or against the outline boundary) do **not** overlap; tight, boundary-flush packings are legal and often optimal.
- **Outline feasibility**: the sum of all soft minimum areas plus all fixed-module areas must fit within `W·H`; the tool must find a legal packing even when free space is tight.
- **Integer floor centers**: a module's pin center is `(x + w//2, y + h//2)` using integer floor division. Any internal wirelength estimate the tool optimizes should use the same floored centers so its self-reported value matches the scorer's recomputation.
- **Degenerate nets**: a net may connect a module to itself or repeat endpoints; such a net contributes zero or a well-defined nonzero distance and must not crash the tool.
- **Nets referencing unknown modules**: a net naming a module that is neither a declared soft nor fixed module is a violation in the scorer; the tool's own legal output never introduces such references.
- **Whitespace / blank lines**: inputs contain blank lines between sections and may use arbitrary spacing; the parser must skip blank lines and tokenize on whitespace.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The tool MUST accept exactly two command-line arguments — an input file path and an output file path — and be invocable as `tool <input.txt> <output.floorplan>` (matching how the scorer launches it).
- **FR-002**: The tool MUST parse the input grammar: `ChipSize <W> <H>`; `NumSoftModules <n>` followed by `n` lines `SoftModule <name> <area>`; `NumFixedModules <n>` followed by `n` lines `FixedModule <name> <x> <y> <w> <h>`; `NumNets <n>` followed by `n` lines `Net <moduleA> <moduleB> <weight>`. All numeric values are integers. Parsing MUST tolerate blank lines between sections and arbitrary whitespace.
- **FR-003**: The tool MUST assign every soft module an integer position `(x, y)` and an integer shape `(w, h)`, and MUST output every soft module exactly once (full coverage — no soft module missing, none duplicated).
- **FR-004**: Every soft module MUST lie fully inside the fixed outline: `x ≥ 0`, `y ≥ 0`, `x + w ≤ W`, and `y + h ≤ H`.
- **FR-005**: Every soft module's chosen shape MUST satisfy the minimum-area constraint `w · h ≥ area`, where `area` is the value declared for that soft module (the declared area is a lower bound; the realized area may be larger).
- **FR-006**: Every soft module's chosen shape MUST satisfy the aspect-ratio constraint `0.5 ≤ h / w ≤ 2.0` (evaluated within a `1e-9` tolerance).
- **FR-007**: No two modules in the combined set of soft and fixed modules MAY overlap. Two axis-aligned rectangles overlap iff `ax < bx + bw` and `bx < ax + aw` and `ay < by + bh` and `by < ay + ah`; modules whose edges merely touch (shared boundary, zero-area intersection) do not overlap and are legal.
- **FR-008**: The tool MUST treat fixed modules as immovable obstacles at exactly their input `(x, y, w, h)` — it MUST NOT relocate or reshape them, MUST keep soft modules clear of them, and MUST NOT list fixed modules in the output file.
- **FR-009**: The tool MUST write the output file in the required format: an optional first line `Wirelength <value>`, then a line `NumSoftModules <n>`, then `n` lines each of the form `<name> <x> <y> <w> <h>` with the soft module's name as the first token. The output MUST contain only soft modules.
- **FR-010**: When the tool emits the optional `Wirelength <value>` line, the stated value MUST equal the weighted HPWL actually implied by the written floorplan as the scorer recomputes it.
- **FR-011**: The tool MUST minimize the weighted HPWL, defined as `Σ weight · (|cx₁ − cx₂| + |cy₁ − cy₂|)` over all nets, where each module's pin center is `(x + w//2, y + h//2)` using integer floor division, and a net's two endpoints may each be a soft module or a fixed module.
- **FR-012**: The tool's output MUST be judged **valid** by the scorer for all 5 provided testcases (`sample`, `public1`–`public4`).
- **FR-013**: The tool MUST finish each testcase within the runtime budget of approximately 600 seconds.
- **FR-014**: The tool MUST exit with status code 0 on success; the scorer treats any non-zero exit as a failed run.
- **FR-015**: The tool MUST be deterministic enough that re-running on the same input yields a floorplan of equal-or-better wirelength and identical legality (no flaky legality failures across runs).

### Key Entities

- **Chip outline**: the fixed placement region, an axis-aligned rectangle of width `W` and height `H` anchored at the origin `(0, 0)`. Every module must lie within `[0, W] × [0, H]`.
- **Soft module**: a deformable block identified by name, with a declared **minimum area**. Its width and height are chosen by the tool (integers) subject to `w·h ≥ area` and `0.5 ≤ h/w ≤ 2`; its position is chosen by the tool. Contributes a single pin at its center.
- **Fixed module**: a block whose name, position `(x, y)`, and size `(w, h)` are pinned by the input. It is an obstacle for placement and a fixed anchor for wirelength, and never appears in the output.
- **Net**: a weighted connection between exactly two modules (`A`, `B`, `weight`). Either endpoint may be a soft or a fixed module. Contributes `weight · (|Δcx| + |Δcy|)` to the wirelength, using the modules' integer-floor centers.
- **Pin / center**: each module's single connection point, located at its geometric center computed with integer floor division `(x + w//2, y + h//2)`.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001 (mandatory legality gate)**: All 5 testcases (`sample`, `public1`–`public4`) are reported `valid=OK` by `scorer/lib/floorplanning.py` — zero completeness, out-of-outline, shape (area / aspect-ratio), and overlap violations.
- **SC-002 (mandatory scoring gate)**: For every public testcase, the scorer's recomputed weighted HPWL is strictly below that case's **Max** zero-score threshold.
- **SC-003 (headline target)**: For every public testcase, weighted HPWL is at or below the case's **Min** target:

  | testcase | Min (target, ≤) | Reference (baseline) | Reference runtime | Max (zero-score) |
  |----------|----------------:|---------------------:|------------------:|-----------------:|
  | public1  | 161,609,972     | 239,984,392          | 581.41s           | 349,768,634      |
  | public2  | 20,966,863      | 38,494,434           | 45.86s            | 41,569,628       |
  | public3  | 1,856,276       | 2,621,582            | 111.55s           | 5,045,921        |
  | public4  | 63,024,850      | 137,686,350          | 285.35s           | 201,625,050      |

- **SC-004 (baseline-competitiveness gate)**: Weighted HPWL is at or below the **Reference** value on every public case, and strictly below it on at least one case — demonstrating the self-written solution beats the human baseline somewhere. (Per the project's R1 rule, a self-written implementation is retained as long as it beats Reference on *any* case; falling back to the reference implementation is required only when *every* case is worse.)
- **SC-005 (runtime)**: Each testcase completes within ~600 seconds.
- **SC-006 (self-report consistency)**: Wherever the output states a `Wirelength` value, it equals the scorer-recomputed weighted HPWL for that case.

## Assumptions

- **Scorer is authoritative**: `scorer/lib/floorplanning.py` is the sole authority for legality and the wirelength metric. The full problem statement (`reference/spec.pdf`) is assumed consistent with it; where they diverge, the scorer wins.
- **Integer coordinates and dimensions**: all positions and sizes in both input and output are integers (the scorer parses them with integer conversion and computes centers with floor division). Non-integer output is out of scope.
- **Declared area is a minimum**: a soft module's declared area is a lower bound on its realized `w·h`; there is no maximum-area or exact-area rule beyond what the outline and non-overlap constraints impose.
- **Aspect-ratio window**: the only shape constraint is `0.5 ≤ h/w ≤ 2` (compared with a `1e-9` tolerance); there is no separate minimum-dimension or fixed-orientation rule.
- **Fixed modules are trusted and excluded from output**: fixed-module rectangles are taken verbatim from the input, assumed mutually non-overlapping and inside the outline as given, and are never written to the output file (the scorer reads them only from the input).
- **Pins at centers**: every module connects through a single pin at its integer-floor center; there are no per-module pin offsets.
- **Output ordering is free**: the order of the soft-module lines does not affect legality or wirelength; the scorer matches modules by name.
- **Invocation**: the scorer launches the tool as `[exe, input_path, output_path]` and matches output files by case name with extension `.floorplan`.
- **Input well-formedness**: inputs follow the documented grammar; the tool need not defend against adversarially malformed inputs beyond skipping blank lines and tolerating whitespace.
- **Runtime budget**: ~600 seconds per testcase on a standard multi-core machine (the human reference reached ~581s on the largest-outline case); the scorer additionally enforces a hard process timeout (1200s) as a safety ceiling.
- **`sample` has no published threshold**: `sample` is a tiny correctness demo (2 soft modules) used only for the legality gate; the Min / Reference / Max targets apply to `public1`–`public4`.
