# experiments — 001-partitioning

每個模型的實作獨立一個子資料夾：`experiments/<model>/`（例如 `qwen3-coder-480b/`、`qwen3-coder-30b/`）。

## 每個 `<model>/` 應包含

- 實作原始碼（依 SDD `tasks.md` 完成）。
- build 說明（或 Makefile）。
- `RESULT.md` — 評測紀錄，模板如下。

## `RESULT.md` 模板

```markdown
# <model> — 001-partitioning 結果

- 模型 / 版本：
- SDD 產物（本模型自產）：見 ./spec/（spec.md / plan.md / tasks.md）
- 編譯指令：
- 執行指令：

| testcase | 合法(scorer) | cut size | 執行時間 |
|----------|----------|----------|----------|
| sample   | OK/NG | | |
| public1  |          |          |          |
| ...      |          |          |          |

- 開發回合數 / 人工介入次數：
- 備註（卡關、偏離規格之處）：
```

> 比較原則見專案根 `.specify/memory/constitution.md`：每個模型從**同一題目敘述**（`reference/spec.pdf` + `benchmark/`）各自產生 spec（放 `./spec/`），唯一變因是模型。
