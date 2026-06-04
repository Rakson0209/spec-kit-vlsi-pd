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

## 實驗

各模型實作放 [`experiments/<model>/`](experiments/)，SDD 產物放 [`spec/`](spec/)。
最佳化指標：**HPWL**（越小越好），須通過 verifier。
