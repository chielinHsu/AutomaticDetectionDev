# {{PROJECT_NAME}} — AI Context

> 只寫「這個專案的事實」。工作流程在 `.agents/harness/engineering-workflow.md`，規則在 `.agents/harness/project-rules.md`，不要在此複製。
> 標示 `TODO(project)` 的地方，建立專案時填寫；用不到的段落直接刪除。建立日期：{{DATE}}

## 專案用途

TODO(project): 一到三句話說明這個專案要解決什麼問題、給誰用。

## 技術棧

| 層 | 技術 | 版本／備註 |
|---|---|---|
| 語言 | TODO(project) | |
| 框架 | TODO(project) | |
| 資料／儲存 | TODO(project) | |
| 平台／部署 | TODO(project) | |

## 重要目錄

| 路徑 | 內容 |
|---|---|
| `src/` | TODO(project) |
| `tests/` | TODO(project) |
| `docs/` | 架構、決策、目前進度、AI 工作紀錄 |
| `.agents/`、`.claude/`、`.ai/`、`scripts/ai/` | AI agent 的規範、skill、交接單與檢查腳本，各檔職責見 `.agents/harness/engineering-workflow.md` 第 0 節 |

## 建置與測試

驗證指令是 agent 判斷「做完了沒有」的唯一依據。沒有寫在這裡的指令，agent 不會自己猜。

| 目的 | 指令 | 備註 |
|---|---|---|
| 建置 | TODO(project) | |
| 單元測試 | TODO(project) | |
| 執行 | TODO(project) | |

`VERIFIED` 的定義：TODO(project): 列出哪些檢查全部成功才算驗證完成。

## 限制與不變條件

- TODO(project): 不可破壞的行為、相容性、效能或安全限制。

## Context Routing（任務 → 先讀什麼）

只載入與任務相關的文件，不要一次讀完。

| 任務 | 必讀文件／skill | 注意 |
|---|---|---|
| 接續上一位 agent 的工作 | `.ai/handoff.md`、`docs/changelog-ai.md` | 先看 `git status`，別覆蓋未完成的修改 |
| 調整架構、新增模組 | `docs/architecture.md`、`docs/decisions.md` | 已記錄的決定沒有新證據不推翻 |
| 收工、換 agent | `ai-handoff` skill | |
| TODO(project) | TODO(project) | |

## 詞彙表

| 詞 | 意義 |
|---|---|
| TODO(project) | |

## 範圍外

- TODO(project): 明確不做的事，避免 agent 順手擴張。
