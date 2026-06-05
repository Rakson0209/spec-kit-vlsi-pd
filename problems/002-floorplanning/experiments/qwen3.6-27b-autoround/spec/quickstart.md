# Quickstart: 固定輪廓平面規劃 — 驗證指南

## Prerequisites

- **Compiler**: `g++` 支援 `-std=c++11`
- **Python**: 3.7+（用於 `scorer/score.py`）
- **Test data**: `problems/002-floorplanning/benchmark/testcase/`（sample + public1~4）

## Build

```bash
cd problems/002-floorplanning/experiments/qwen3.6-27b-autoround
make
```

編譯後產生 `bin/hw3` 執行檔。

## Run & Validate

### Step 1: 單筆 testcase 驗證（sample）

```bash
# 執行
bin/hw3 ../../benchmark/testcase/sample.txt output/sample.floorplan

# 查看輸出
cat output/sample.floorplan

# 計分（合法性 + HPWL）
python scorer/score.py 002 --output-dir output --cases sample
```

**Expected**: 合法性 = OK，wirelength ≤ 已知最佳值

### Step 2: 所有 testcase 驗證

```bash
# 對所有 testcase 執行
for case in sample public1 public2 public3 public4; do
  bin/hw3 ../../benchmark/testcase/${case}.txt output/${case}.floorplan
done

# 批次計分
python scorer/score.py 002 --output-dir output --md ../result.md
```

### Step 3: 官方 verifier 驗證（需 Linux）

```bash
../../benchmark/verifier/verify ../../benchmark/testcase/sample.txt output/sample.floorplan
```

**Expected**: `[Success] Your output file satisfies our basic requirements.`

## Success Criteria

| Criterion | Target |
|-----------|--------|
| 所有 testcase 合法 | verifier = Success |
| public1 wirelength | ≤ 161,609,972 |
| public2 wirelength | ≤ 20,966,863 |
| public3 wirelength | ≤ 1,856,276 |
| public4 wirelength | ≤ 63,024,850 |
| 執行時間 | 每筆 ≤ 600s |

## Development Workflow

1. **Local validation**（Windows 可用）: `scorer/score.py` 提供完整的合法性檢查與 HPWL 計算
2. **Official verification**（需 Linux/WSL）: `benchmark/verifier/verify`
3. **Iterate**: 修改 src/ → `make` → 執行 → 計分 → 調整 SA 參數

## Notes

- 輸入/輸出格式詳見 `spec/contracts/`
- 資料模型詳見 `spec/data-model.md`
- 演算法設計詳見 `spec/research.md`
