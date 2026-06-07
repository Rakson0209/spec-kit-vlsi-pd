# Contract: Input / Output File Format

**Spec**: [../spec.md](../spec.md) (FR-002, FR-006) · Source of truth: [`scorer/lib/partitioning.py`](../../../../../../scorer/lib/partitioning.py) `_parse_input` / `_parse_output`

> Parsing rule (scorer `read_token_lines`): the file is read line-by-line, each line `split()` on whitespace; **blank lines are skipped**; only token position matters. Sections appear in the fixed order below.

## Input `.txt` grammar

```
NumTechs <n>
Tech <techName> <numLibCells>
LibCell <libName> <width> <height>      # × numLibCells
...                                      # × NumTechs Tech blocks
DieSize <W> <H>
DieA <techName> <util%>
DieB <techName> <util%>
NumCells <n>
Cell <cellName> <libCellName>           # × NumCells
NumNets <n>
Net <netName> <degree>
Cell <cellName>                          # × degree
...                                      # × NumNets Net blocks
```

Notes:
- `<width> <height>` parse as floats; area = `width × height`. The **same `<libName>` recurs in every Tech** with possibly different dimensions.
- `<util%>` is a percentage → cap fraction = `util% / 100`.
- Die physical area = `W × H` (shared by DieA and DieB).
- Blank lines may separate sections (as in `sample.txt`); arbitrary inter-token whitespace allowed.
- Inputs are well-formed per this grammar (spec Assumptions) — no need to defend against malformed files beyond blank-line/whitespace tolerance.

See [`benchmark/testcase/sample.txt`](../../../../benchmark/testcase/sample.txt) (2 techs, 8 cells, 6 nets).

## Output `.out` grammar

```
CutSize <n>            # OPTIONAL first line; case-insensitive token "cutsize"
DieA <countA>
<cellName>             # × countA  (one cell name per line)
DieB <countB>
<cellName>             # × countB
```

Rules enforced by the scorer:
- The scorer reads only the **first token** of each cell line → the cell name MUST be first (extra tokens ignored, but emit just the name).
- `countA` / `countB` MUST equal the number of name lines that follow each header.
- **Coverage** (else illegal): every input cell appears exactly once across DieA∪DieB; no unknown names; no duplicates.
- **Order of names within a list is irrelevant** to legality and cut.
- If `CutSize <n>` is emitted, `n` MUST equal the cut the scorer recomputes (FR-007/SC-006). The scorer records it as "self-reported" but always **recomputes** the official metric.
- The reference emits `CutSize <n>` (capital C/S); any case works.

## Legality & metric (scorer, R6)

- **Legal** iff: full exclusive coverage **and** `areaA/(W·H) ≤ capA + 1e-9` **and** `areaB/(W·H) ≤ capB + 1e-9`, where `areaA` sums DieA cells' areas **in DieA's Tech** (likewise B).
- **Metric** `cut_size` = number of nets with ≥1 cell on A **and** ≥1 cell on B (lower is better).
