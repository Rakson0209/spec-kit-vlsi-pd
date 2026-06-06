# Tasks: 001 Partitioning — qwen3.6-27b-autoround

## R1 Fallback：直接用 reference code

reference code 是已知可通過所有 testcase 的 baseline。直接用它在基礎上優化，不要從頭寫。

- [ ] T001 複製 `reference/src/main.cpp` → `main.cpp`
- [ ] T002 修改 `main.cpp` 中 Boost 引用為 STL（`boost::unordered_map` → `std::unordered_map`，刪除 boost include）
- [ ] T003 編譯：`g++ -std=c++20 -O3 -fopenmp -pthread -o hw2 main.cpp`
- [ ] T004 對所有 testcase 執行 + scorer 驗證，確認全部 OK
- [ ] T005 記錄 baseline cut size 數值

## 優化：每次改動後驗證全 case

- [ ] T006 替換 greedy partition 為 FM initial + SA refinement
- [ ] T007 加入 OpenMP 多起點搜尋
- [ ] T008 逐 testcase 調參，每次改動後跑全 case scorer
- [ ] T009 最終驗證：所有 case 合法 + cut size ≤ Min

## 規則
- 每次改 code 後對**所有** testcase 跑 scorer
- 任何 case NG → 立刻修，不繼續優化
- **全部** case 都比 reference 差 → 才改用 reference code。只要有任何 case 優於 reference，就繼續用自己 code
