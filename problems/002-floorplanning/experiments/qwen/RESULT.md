# qwen — 002-floorplanning 結果

- 模型 / 版本：Qwen (SDD-driven, C++17)
- SDD 產物：見 ../../spec/
- 編譯指令：`g++ -std=c++17 -O3 -static -o hw.exe main.cpp parser.cpp placement.cpp optimizer.cpp hpwl.cpp output.cpp`
- 執行指令：`hw3 <input.txt> <output.floorplan> [seed]`

| testcase | verifier | 線長/面積 | 執行時間 |
|----------|----------|-----------|----------|
| sample   | OK       | 215       | <1s      |
| public1  | OK       | 245274640 | ~60s     |
| public2  | OK       | 34550821  | ~60s     |
| public3  | OK       | 3082522   | ~60s     |
| public4  | OK       | 117192850 | ~60s     |

- 開發回合數 / 人工介入次數：1 (SDD-driven, with iterative bug fixes)
- 備註：
  - 使用矩形交集 + interval tree 取代 2D grid，支援大晶片尺寸
  - 模擬退火 SA (swap/move/reshape) 優化，540s timeout
  - `-static` 連結以解決 MinGW 動態庫相容性問題
  - 初始放置使用隨機探針 + compaction
  - SA 對小 testcase (sample) 有效改善；大 testcase 因 SA 迭代數不足仍使用初始放置
