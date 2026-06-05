# Quickstart: 驗證 Fixed-outline Floorplanning（claude-opus-4-8）

> 端到端驗證指南：build → run → verify → score。實作細節見 plan/research/data-model，本檔只證明能跑通。

## 前置

- C++11 編譯器（`g++ -std=c++11 -O3`）。
- Windows 計分：Python + `scorer/score.py`。
- Linux 驗證：`benchmark/verifier/verify`（如遇權限：`chmod +x verify`）。
- 工作目錄：`problems/002-floorplanning/experiments/claude-opus-4-8/`。

## 1. 編譯

```sh
cd problems/002-floorplanning/experiments/claude-opus-4-8
g++ -std=c++11 -O3 -o hw3 main.cpp
# 或： make
```
預期：產生 `hw3`，無警告錯誤。

## 2. 跑 sample（冒煙測試）

```sh
./hw3 ../../benchmark/testcase/sample.txt out_sample.floorplan
```
預期：產生 `out_sample.floorplan`，首行 `Wirelength <值>`，含 2 個 soft module（GPU、CPU）。

## 3. 驗證合法性（Linux，真理）

```sh
../../benchmark/verifier/verify ../../benchmark/testcase/sample.txt out_sample.floorplan
```
預期：`[Success] Your output file satisfies our basic requirements.`

## 4. 跑全部 public 並計分（Windows）

```powershell
foreach ($t in "public1","public2","public3","public4") {
  .\hw3 ..\..\benchmark\testcase\$t.txt "$t.floorplan"
  python ..\..\..\..\scorer\score.py ..\..\benchmark\testcase\$t.txt "$t.floorplan"
}
```
逐一記錄 HPWL 與 wall-clock 時間。

## 5. 驗收對照（目標）

| 檢查 | 通過條件 |
|------|---------|
| 合法性 | sample + public1~4 全部 `[Success]` |
| HPWL（主目標） | public1≤161,609,972；public2≤20,966,863；public3≤1,856,276；public4≤63,024,850 |
| HPWL（保底） | 任一未達 Min 時，須 < Reference 且 < Max（見 research.md 門檻表） |
| 時間 | 每份 ≤ 600s；public2/3/4 目標 ≤ 120s |
| 重現性 | 同硬體重跑同測資 HPWL 差異 ≤ 5%（記錄 seed） |

## 6. 紀錄

將上述數值填入 `experiments/claude-opus-4-8/RESULT.md`（含模型版本、編譯/執行指令、各測資
verifier 結果、HPWL、時間、開發回合數、seed），對照 Min / Reference / Max。
