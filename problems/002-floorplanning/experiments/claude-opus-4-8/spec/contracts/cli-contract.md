# Contract: CLI / 執行介面（claude-opus-4-8）

## 命令列

```
hw3 <input.txt> <output.floorplan>
```

| 項目 | 約定 |
|------|------|
| argv[1] | 輸入測資 `.txt` 路徑 |
| argv[2] | 輸出 `.floorplan` 路徑 |
| 互動 | 無（批次、不讀 stdin、不需參數檔） |
| 回傳碼 | 0 成功 |
| stdout/stderr | 不影響評測（可空或印進度）；結果只看輸出檔 |
| 多測資 | 同一二進位、同一編譯設定跑全部 5 份，不逐測資改碼 |
| 時間 | 單份 ≤ 600s（內部計時建議在 ~580s 收斂並輸出） |
| 決定性 | 若用隨機演算法須固定 seed，使同硬體重跑 HPWL 差異 ≤ 5%（記錄 seed） |

## 編譯

```
g++ -std=c++11 -O3 -o hw3 main.cpp
```

## 驗證 / 計分契約

- 正確性：`benchmark/verifier/verify <input.txt> <output.floorplan>` → 需印 `[Success]`（Linux）。
- 計分：`python scorer/score.py <input.txt> <output.floorplan>` → 取 HPWL 數值（Windows）。
- verifier `[Success]` 為唯一正確性真理；未過即視為未通過，無論程式看似多合理。
