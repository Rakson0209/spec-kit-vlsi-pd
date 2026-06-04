# Quickstart: 固定輪廓平面規劃

**Date**: 2026-06-04

## 環境需求

- **Compiler**: g++ 4.8+（支援 C++11）
- **Python**: 3.7+（用於計分器）
- **OS**: Windows (MinGW/MSVC) / Linux

## 編譯

```sh
cd problems/002-floorplanning/experiments/qwen/src
make
```

編譯後產生 `../bin/hw3`（或 Windows 上的 `hw3.exe`）。

或直接編譯：
```sh
g++ -std=c++11 -O3 -o hw3 main.cpp parser.cpp placement.cpp optimizer.cpp hpwl.cpp output.cpp
```

## 執行

```sh
# 單一 testcase
./hw3 ../../benchmark/testcase/sample.txt out.floorplan

# 指定 random seed（可選）
./hw3 ../../benchmark/testcase/sample.txt out.floorplan 42
```

命令列格式：`hw3 <input.txt> <output.floorplan> [seed]`

## 驗證

```sh
# 使用 Python 計分器（Windows 可跑）
python scorer/score.py 002 --output-dir problems/002-floorplanning/experiments/qwen/out --label qwen

# 使用官方 verifier（需 Linux）
problems/002-floorplanning/benchmark/verifier/verify benchmark/testcase/sample.txt out.floorplan
```

## 批次執行

```sh
# 讓 scorer 自動跑所有 testcase
python scorer/score.py 002 --run problems/002-floorplanning/experiments/qwen/bin/hw3 \
    --output-dir problems/002-floorplanning/experiments/qwen/out \
    --label qwen --md docs/002-qwen.md --csv docs/002-qwen.csv
```

## 輸出範例（sample.txt）

```
Wirelength 215

NumSoftModules 2
GPU 0 0 5 5
CPU 5 0 3 5
```

## 目錄結構

```
experiments/qwen/
├── src/            # 原始碼
├── bin/            # 編譯產物 (hw3)
├── out/            # 執行輸出 (.floorplan)
└── RESULT.md       # 評測紀錄
```
