# Quickstart Validation Guide

**Feature**: Fixed-outline Floorplanning Optimizer
**Implementation target**: `problems/002-floorplanning/experiments/claude-sonnet-4-6/`
**Date**: 2026-06-05

---

## Prerequisites

- **Compiler**: `g++` with C++20 support (`g++ -std=c++20 -O3`)
- **Python 3** (for scorer)
- **Linux** (for official verifier binary); Windows-native scorer works on all platforms

---

## Build

```sh
cd problems/002-floorplanning/experiments/claude-sonnet-4-6
g++ -std=c++20 -O3 -o hw3 main.cpp
```

Expected: binary `hw3` created with no warnings.

---

## Scenario 1: Sample Testcase (Sanity Check)

**Purpose**: Verify basic correctness on the toy 2-module example.

```sh
# From: problems/002-floorplanning/experiments/claude-sonnet-4-6/
./hw3 ../../benchmark/testcase/sample.txt out_sample.floorplan
```

**Expected outcome**:
- `out_sample.floorplan` created
- Contains `NumSoftModules 2` with both GPU and CPU entries
- Each module within chip bounds (8×7)
- No overlap between GPU, CPU, PAD1, FIXED1

**Verify with scorer** (Windows):
```sh
# From repo root
python scorer/score.py problems/002-floorplanning/benchmark/testcase/sample.txt \
       problems/002-floorplanning/experiments/claude-sonnet-4-6/out_sample.floorplan
```

**Expected scorer output**:
```
valid: True
wirelength: <some number>
```

---

## Scenario 2: Linux Verifier Check

**Purpose**: Official legality gate.

```sh
# On Linux, from repo root
problems/002-floorplanning/benchmark/verifier/verify \
  problems/002-floorplanning/benchmark/testcase/sample.txt \
  problems/002-floorplanning/experiments/claude-sonnet-4-6/out_sample.floorplan
```

**Expected**: `[Success]`

---

## Scenario 3: Public Testcase Batch Run

**Purpose**: Measure HPWL against Min thresholds.

```sh
for tc in public1 public2 public3 public4; do
  echo "=== $tc ==="
  time ./hw3 ../../benchmark/testcase/${tc}.txt out_${tc}.floorplan
  python ../../scorer/score.py ../../benchmark/testcase/${tc}.txt out_${tc}.floorplan
done
```

**Expected outcomes** (target):

| testcase | valid | HPWL target | Runtime target |
|----------|-------|------------|----------------|
| public1  | True  | ≤ 161,609,972 | ≤ 580s |
| public2  | True  | ≤ 20,966,863  | ≤ 580s |
| public3  | True  | ≤ 1,856,276   | ≤ 580s |
| public4  | True  | ≤ 63,024,850  | ≤ 580s |

**Minimum acceptable** (if Min is not reached):

| testcase | Reference HPWL (must beat) |
|----------|--------------------------|
| public1  | < 239,984,392 |
| public2  | < 38,494,434  |
| public3  | < 2,621,582   |
| public4  | < 137,686,350 |

---

## Scenario 4: Output Format Validation

**Purpose**: Confirm output matches the contract in [contracts/output-format.md](contracts/output-format.md).

Check manually:
1. First non-blank line: `Wirelength <integer>`
2. Following: `NumSoftModules <n>`
3. Exactly `n` module lines of format `<name> <x> <y> <w> <h>` (all integers)
4. All soft module names from input are present
5. No fixed module names appear in output

---

## Recording Results

After a successful run, record in `RESULT.md` (to be created in the implementation directory):

```markdown
| testcase | valid | HPWL | vs Min | vs Reference | runtime |
|----------|-------|------|--------|--------------|---------|
| public1  | ✅    | xxx  | -xx%   | -xx%         | xxxs    |
```

See Constitution Principle IV (Reproducibility) and V (Quantify Quality).
