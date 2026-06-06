# Implementation Plan: [FEATURE]

**Date**: [DATE] | **Spec**: [link]

## Technical Context

**Language/Version**: C++20
**Compiler**: `g++ -std=c++20 -O3 -fopenmp -pthread -I tools/boost`（內建 portable `tools\mingw64\`）
**Primary Dependencies**: Boost（`boost::graph`, `boost::geometry`）、OpenMP、pthread
**Platform**: Windows native

## Constitution Check

| 守則 | 狀態 |
|------|:---:|
| R1 Baseline Fallback — 寫 code → scorer → 比 baseline 差就複製 reference/src/ | ⬜ |
| R2 Boost + 平行 — 使用 `-fopenmp -pthread -I tools/boost` | ⬜ |
| R3 合法性 — 所有 testcase scorer OK | ⬜ |
| R4 全 case — 每次改動驗證所有 testcase | ⬜ |
| R5 規格先行 — spec → plan → tasks → code | ⬜ |
| R6 評測即真理 — scorer 為唯一標準 | ⬜ |

## Project Structure

```
experiments/<model>/
├── main.cpp
├── Makefile
├── spec/           # SDD 產物
└── out/            # 輸出檔
```

## Complexity Tracking

> Fill only if Constitution Check has violations

| Violation | Why | Rejected Alternative |
|-----------|-----|---------------------|
