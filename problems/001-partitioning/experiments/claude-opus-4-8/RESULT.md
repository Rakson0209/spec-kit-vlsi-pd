# 001 Partitioning — claude-opus-4-8 實作結果

**Date**: 2026-06-07 · **Spec**: [spec/spec.md](./spec/spec.md) · **Scorer**: `scorer/lib/partitioning.py`（R6 唯一真理）

## 演算法

自寫 solver（無 Boost，`g++ -std=c++20 -O3 -fopenmp -pthread -static`）：

1. **快速 parser**：整檔讀入 + 手寫 tokenizer，cell 名稱 intern 成連續 int id；net→cell / cell→net 雙向 CSR；net 內去重、丟棄 distinct degree < 2 的 net。
2. **可行初分割**：cell 依 `max(areaA,areaB)` 由大到小，放到相對較空（`(used+area)/cap` 較小）且容量允許的 die；含 feasibility repair 保底（R3）。
3. **面積受限 FM**：bucket-gain（`O(1)` 取最大/更新）、`countA/countB` 增量維護、pass 內記錄最佳前綴並回滾（roll-back-to-best）；移動可行性用 `dest used + areaDest ≤ cap + 1e-9` 對齊 scorer。
4. **Multilevel（大 case）**：heavy-edge matching coarsen → 粗階多起點 greedy+FM → uncoarsen 各階 FM 精修；以隨機化 coarsening 順序的多重 V-cycle 取最佳合法解。
5. **平行 multi-start**（R2）：OpenMP 平行跑多個 seed（小 case：flat multi-start FM；大 case：多 V-cycle），以「合法、最低 cut、tie 取最小 seed」歸併；~285s wall-clock deadline，逾時輸出當前最佳合法解。

小 case（≤3000 cell）走 flat multi-start FM（固定 seed 全跑 → 完全可重現）；大 case 走 multilevel，停止條件為 deadline 或連續 800 次 V-cycle 無改善。

## 計分結果（全部由 scorer `--run` 重新執行產生）

| testcase | cut（ours） | Min（目標 ≤） | Reference | Max（零分） | vs Ref | vs Min | 合法 |
|----------|-----------:|------------:|----------:|-----------:|:------:|:------:|:----:|
| public1  | **107**    | 104         | 193       | 1,441      | −45%   | +2.9%  | OK |
| public2  | **858**    | 816         | 3,666     | 27,862     | −77%   | +5.1%  | OK |
| public3  | **1,128**  | 1,762       | 7,092     | 103,659    | −84%   | **−36%** | OK |
| public4  | **1,797**  | 982         | 2,265     | 12,421     | −21%   | +83%   | OK |
| public5  | **702**    | 297         | 1,669     | 48,964     | −58%   | +136%  | OK |
| public6  | **4,836**  | 5,159       | 10,281    | 490,120    | −53%   | **−6.3%** | OK |
| sample   | 1          | —           | —         | —          | —      | —      | OK |

- **SC-001（合法性硬門檻）**：7/7 `valid=OK`。✅
- **SC-002（< Max）**：所有 public case 遠低於 Max。✅
- **SC-004（打敗 baseline）**：**所有** public case 皆 ≤ Reference，且至少一個 `<`（實際上全部 `<`）。✅
- **SC-003（≤ Min headline）**：public3、public6 **低於 Min**；public1、public2 在 Min +~5% 內；public4、public5 高於 Min 但遠低於 Reference。部分達成。
- **SC-005（runtime）**：所有 case ≤ ~285s deadline，0 退出。✅
- **SC-006（自報一致）**：`CutSize` 行等於 scorer 重算值。✅

## R1 Baseline Fallback 決策

自寫 solver 在**全部** 6 個 public case 皆優於 `reference/src`（Reference 欄），**未觸發** fallback → **保留自寫實作**，未複製/移植 reference。

## 決定論（FR-013）

- 小 case：固定 seed 全跑，完全可重現。
- 大 case：V-cycle seed 為索引的確定函數，歸併以「最低 cut、tie 取最小 seed」破平手；多執行緒/時間影響的只有「跑幾輪」，重跑為**等價或更佳**。實測 public1=107、public4=1797 重跑結果一致。

## 編譯 / 執行

```powershell
. .\tools\mingw64\setup-env.ps1
g++ -std=c++20 -O3 -fopenmp -pthread -static -static-libgcc -static-libstdc++ -o hw2 main.cpp
.\hw2.exe <input.txt> <output.out>
python scorer\score.py 001 --run .\hw2.exe --output-dir out --label claude-opus-4-8
```

> 採 `-static*`：scorer 以 subprocess 執行 exe，未載入 mingw runtime PATH；靜態連結 libgcc/libstdc++/libgomp 避免缺 DLL（entrypoint not found）。
> 無任何 Boost 依賴。報表：[score.md](./score.md)、[score.csv](./score.csv)。
