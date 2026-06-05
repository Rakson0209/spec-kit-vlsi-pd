# experiments — 002-floorplanning

每個模型的實作獨立一個子資料夾：`experiments/<model>/`。

## 每個 `<model>/` 應包含

- 實作原始碼（依 SDD `tasks.md` 完成）。
- build 說明（或 Makefile）。
- `RESULT.md` — 評測紀錄，模板如下。

## `RESULT.md` 模板

```markdown
# <model> — 002-floorplanning 結果

- 模型 / 版本：
- SDD 產物（本模型自產）：見 ./spec/
- 編譯指令：
- 執行指令：

| testcase | 合法(scorer) | 線長/面積 | 執行時間 |
|----------|----------|-----------|----------|
| sample   | OK/NG | | |
| public1  |          |           |          |
| ...      |          |           |          |

- 開發回合數 / 人工介入次數：
- 備註：
```

> 比較原則見專案根 `.specify/memory/constitution.md`。
