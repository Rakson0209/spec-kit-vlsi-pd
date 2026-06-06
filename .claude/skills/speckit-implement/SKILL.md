---
name: speckit-implement
description: Execute implementation. Use after speckit-tasks.
metadata:
  short-description: Execute implementation
  triggers: speckit.implement, /speckit.implement, implement, execute tasks
---

# speckit-implement

## 核心流程（嚴格遵守）

### Step 1: 自寫 code → 一輪定生死
1. 讀 `tasks.md`，照 tasks 寫自己的 C++ code
2. 編譯：`g++ -std=c++20 -O3 -fopenmp -pthread -I tools/boost -o hw3 main.cpp`
3. 對**所有** testcase 跑 scorer：
   ```powershell
   python scorer/score.py <problem-num> --output-dir <out-dir> --label <model>
   ```

### Step 2: 判定（R1 — 憲章最高原則）
| 結果 | 行動 |
|------|------|
| 全部合法 + 全部指標 ≤ baseline | → Step 4（優化） |
| 全部 case 指標都 > baseline | → **Step 3**（複製 reference） |
| 部分 case 優於 baseline、部分差 | → **繼續用自己的 code 優化**，不許 fallback |

### Step 3: Fallback — 複製 reference/src/
```powershell
Copy-Item -Recurse reference/src/* experiments/<model>/
g++ -std=c++20 -O3 -fopenmp -pthread -o hw3 *.cpp
# 對所有 testcase 跑 scorer，確認全部 OK
```
在 reference code 基礎上優化。**不允許回頭改自己的 code。**

### Step 4: 優化 — 每次改動驗證全 case
- 改 code → 編譯 → 對**所有** testcase 跑 scorer
- 有 NG → 先修合法性
- 全部 OK → 再比指標
- **禁止**只盯著一個 case

## 編譯指令（一律使用）
```powershell
. .\tools\mingw64\setup-env.ps1
g++ -std=c++20 -O3 -fopenmp -pthread -I tools/boost -o hw3 main.cpp
```

## 驗證
```powershell
python scorer/score.py <problem-num> --output-dir <out-dir> --label <model>
```

## 完成條件
- [ ] 所有 testcase scorer 合法（OK）
- [ ] 指標 ≤ baseline（reference/src/）
- [ ] RESULT.md 記錄所有數據
