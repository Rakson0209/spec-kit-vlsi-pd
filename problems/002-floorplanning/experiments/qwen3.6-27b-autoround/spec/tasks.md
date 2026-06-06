# Tasks: 002 Floorplanning — qwen3.6-27b-autoround

## R1 Fallback：直接用 reference code

- [ ] T001 複製 `reference/src/main.cpp` → `main.cpp`
- [ ] T002 編譯：`g++ -std=c++20 -O3 -fopenmp -pthread -o hw3 main.cpp`
- [ ] T003 對所有 testcase 執行 + scorer 驗證，確認全部 OK
- [ ] T004 記錄 baseline HPWL 數值

## 優化：每次改動後驗證全 case

- [ ] T005 替換 packing 為 B*-tree + SA
- [ ] T006 加入 OpenMP 多起點搜尋
- [ ] T007 逐 testcase 調參
- [ ] T008 最終驗證：所有 case 合法 + HPWL ≤ Min

## 規則
- 每次改 code 後對**所有** testcase 跑 scorer
- 任何 case NG → 立刻修
- 任一 case 指標比 reference 差 → 回到 reference code
