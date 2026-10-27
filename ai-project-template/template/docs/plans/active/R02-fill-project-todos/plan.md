---
id: R02
title: 填寫所有 TODO(project)
type: 雜項
status: 規劃中
created: {{DATE}}
updated: {{DATE}}
depends_on: [R01]
related: []
branch:
---

# R02 — 填寫所有 `TODO(project)`

## 目標

把範本裡要專案自己填的事實與規則填完，`scripts/ai/check-ai-structure.ps1 -Strict` 通過。

## 範圍

- `AI_CONTEXT.md`、`.agents/harness/project-rules.md`、`MODEL_ROUTING.md`「本專案案例」、`docs/00-master-plan.md`、`docs/architecture.md`。

## 不做

- 實作功能；第一個功能需求另開計畫（`docs/plans/INDEX.md` 的下一個編號）。

## 細項

| 編號 | 細項 | 狀態 | 分支／commit | 驗證 |
|---|---|---|---|---|
| R02.1 | `AI_CONTEXT.md`：用途、技術棧、建置與測試指令 | ⬜ | | |
| R02.2 | `project-rules.md`：風格、Git 授權範圍、不提交的路徑 | ⬜ | | |
| R02.3 | `MODEL_ROUTING.md`「本專案案例」與 `00-master-plan.md`、`architecture.md` | ⬜ | | |

圖例：✅ 完成　⏳ 進行中　⬜ 尚未開始　⚠️ 有未解決問題　⛔ 取消

## 完成定義

- [ ] `scripts/ai/check-ai-structure.ps1 -Strict` 沒有 FAIL
- [ ] `node scripts/ai/check-model-routing.mjs` 沒有 FAIL
- [ ] 細項表全部 ✅；資料夾搬到 `docs/plans/completed/`，`INDEX.md` 已更新

## Start prompt（開新 session 時貼上）

```
你正在 <專案資料夾> 繼續專案。回覆一律使用繁體中文（zh-TW），完成時總結修改內容。
請先讀 AGENTS.md、docs/plans/INDEX.md 與 docs/plans/active/R02-fill-project-todos/ 的 plan.md、status.md，
只做 R02：填寫 TODO(project)，用 scripts/ai/check-ai-structure.ps1 -Strict 確認。
```
