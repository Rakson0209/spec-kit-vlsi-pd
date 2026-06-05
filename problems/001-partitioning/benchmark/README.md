# 001-partitioning 測資說明

## repo 內含

- `testcase/sample.txt` — 小型範例，供快速跑流程。

> 評測與合法性檢查一律用專案根的 `scorer/`（純 Python，Windows 可跑）。

## 不在 repo 內（已由 `.gitignore` 排除）

`testcase/public1.txt` ~ `public6.txt` 為大型測資（單檔最大約 60 MB，合計數百 MB），
不適合進 git。請自行放置於本目錄 `testcase/` 下後再進行實驗。

| 檔案 | 約略大小 |
|---|---|
| public1.txt | 180 KB |
| public2.txt | 3 MB |
| public3.txt | 17 MB |
| public4.txt | 1 MB |
| public5.txt | 9 MB |
| public6.txt | 60 MB |

> 來源：清大 CS6135 VLSI Physical Design Automation 課程 HW2 提供之 public testcase。
