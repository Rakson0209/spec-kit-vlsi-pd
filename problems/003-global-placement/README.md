# 003 — 全域佈局 (Global Placement)

> 清大 CS6135 VLSI Physical Design Automation, HW4。完整題目見 [`reference/spec.pdf`](reference/spec.pdf)。

## 題目

讀入 **Bookshelf** 格式的電路，求每個 cell 的座標，使**半周長線長 (HPWL)** 最小化。

## 輸入格式（Bookshelf）

一組 `.aux` 指向 `.nodes / .nets / .pl / .scl / .wts`：

```
benchmark/testcase/public1/
├── public1.aux      # 入口，列出其餘檔案
├── public1.nodes    # cell 與尺寸
├── public1.nets     # net 連接
├── public1.pl       # 初始座標
├── public1.scl      # row / site 資訊
└── public1.wts      # net 權重
```

## 輸出格式（`.gp.pl`）

各 cell 的最終座標（Bookshelf placement 格式）。

## 編譯與執行 baseline

> `reference/obj/*.o` 是**題目提供的預編譯函式庫**（無原始碼，Makefile 以 `wildcard ../obj/*.o` 連結），
> 編譯必需，請勿刪除。需 `reference/include/` 下的標頭檔。

```sh
cd reference/src
mkdir -p ../bin
make                 # 產生 reference/bin/hw4，連結 ../obj/*.o 與 ../include
../bin/hw4 ../../benchmark/testcase/public1/public1.aux out.gp.pl
```

> 注意：原始 Makefile 的 `make test` target 假設 `../testcase`、`../verifier` 在 `reference/` 下，
> 但本 repo 已將測資與驗證器移到共用的 `benchmark/`。請改用上方手動執行路徑，
> 或自行調整 Makefile 的相對路徑指向 `../../benchmark/`。

## 驗證

```sh
benchmark/verifier/verify <input.aux> <output.gp.pl>
# 通過會印 [Success]，否則印 [Error] ...
```

## Baseline 指標門檻（要超越的目標）

最佳化指標為 **HPWL / wirelength（越小越好）**。下表 **Min 為要超越的門檻**（已知最佳結果），
`Reference` 為人類參考解（baseline）的實測值。runtime 上限約 **590s**（須在此之內完成）。

| testcase | **目標 Min（要 ≤ 此值，越低越好）** | Reference 參考解 | Reference runtime | Max（零分門檻） |
|---|---:|---:|---:|---:|
| public1 | **59,788,412**  | 87,987,694  | 28.43s  | 319,198,465   |
| public2 | **10,530,075**  | 18,642,174  | 58.95s  | 28,999,635    |
| public3 | **395,131,978** | 750,902,922 | 110.7s  | 2,631,834,205 |

> 依憲章原則 VI（研究先行，超越基準）：各模型在 `research.md` 須以上表 **Min** 為要打敗的門檻，
> 並記錄自己各 testcase 的 HPWL 與 runtime，對比 Min / Reference。

## 實驗

每個模型的完整 SDD 鏈與實作放 [`experiments/<model>/`](experiments/)；該模型自產的 SDD 產物放 `experiments/<model>/spec/`。所有模型共用同一份題目敘述（[`reference/spec.pdf`](reference/) + `benchmark/`）。
最佳化指標：**HPWL**（越小越好），須通過 verifier。
