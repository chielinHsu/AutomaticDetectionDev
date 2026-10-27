---
id: R01
title: 套用 AI 共用結構範本（3.0.0）
type: 雜項
status: 完成
created: {{DATE}}
updated: {{DATE}}
depends_on: []
related: []
branch:
---

# R01 — 套用 AI 共用結構範本（3.0.0）

## 目標

讓 Claude Code 與 Codex 在這個專案用同一套入口、工作流程、計畫管理與交接文件。

## 範圍

- 入口（`AGENTS.md`、`CLAUDE.md`）、`AI_CONTEXT.md`、`.agents/harness/`、`ai-handoff` skill、模型路由、`docs/plans/`、ADR、總體計畫、檢查腳本。

## 不做

- 填寫專案事實（`TODO(project)`）：見 R02。

## 細項

| 編號 | 細項 | 狀態 | 分支／commit | 驗證 |
|---|---|---|---|---|
| R01.1 | 由 `New-AiProject.ps1` 建立結構 | ✅ | 範本建立（{{DATE}}） | 建立時跑過 `scripts/ai/check-ai-structure.ps1` |

## 決策

- [ADR-000](../../../adr/000-ai-agent-structure.md)

## 完成定義

- [x] 結構建立，`check-ai-structure.ps1` 沒有 FAIL（`TODO(project)` 未填時 `-Strict` 預期失敗，見 R02）
