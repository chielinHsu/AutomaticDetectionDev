# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Sonnet 5.5)
- 日期：2026-10-05

## 正在做

無。S00 已完成並合併；S01 待開始（依 Step Protocol 在新 session 做）。

## 已完成

- 套用 AI 共用結構（Codex 與 Claude Code）到本專案：`CLAUDE.md` 改為薄入口，原內容搬到 `AI_CONTEXT.md`、`docs/architecture.md`、`.agents/harness/project-rules.md`。
- `docs/STATUS.md` 更名為 `docs/current-status.md`，14 個檔案的引用同步更新。
- 新增 `docs/adr/000-ai-agent-structure.md` 與 `docs/decisions.md` 索引。
- 前三個 commit 的訊息改寫成使用者的 `[作者]屬性:` 格式；`project-rules.md` 的 Commit 規則同步改為該格式，並已合併到 `main`。

## 下一步

- 確認改寫後的歷史沒問題後，刪除備份分支 `backup/before-commit-reformat`。
- 開 S01 新 session：prompt 在 `docs/plan/S01-ux-spec.md` 底部。結構套用已合併到 `main`，prompt 引用的 `AGENTS.md` 與 `docs/current-status.md` 都在 `main` 上。
- 是否 push 到遠端由使用者決定（目前沒有 push）。

## 相關檔案

- `AGENTS.md`、`CLAUDE.md`、`AI_CONTEXT.md`
- `.agents/harness/project-rules.md`（Step Protocol 在此）
- `docs/adr/000-ai-agent-structure.md`

## 不要動

- `ai-project-template/`：未追蹤的範本來源，不修改、不 stage。

## 已知問題

- Codex 端的 AGENTS.md 流程沒有在本機實測過（Codex CLI 0.136.0 無法執行 `codex exec`）。
- 範本的檢查腳本 `*.ps1` 在 `.gitattributes` 下 checkout 會是 CRLF；`git add` 可能出現換行提示，屬預期。
- 12 份 `docs/plan/S*.md` 的「Commit 範本」仍是英文 Conventional 風格；`project-rules.md` 規定它們只當內容參考，commit 格式以規則為準。

## 未提交的變更

- 無。套用結構的變更已 commit（`fd4fd2a`）並以 `--no-ff` 合併到 `main`；沒有 push。
- `ai-project-template/` 仍未追蹤，不屬於那次 commit；它的 `project-rules.md` 已同步改成同一份 commit 格式，也尚未 commit。
- commit 格式規則的變更已合併到 `main`（`--no-ff`）；沒有 push。

## 假設與未驗證項目

- 假設沒有其他進行中的 session：套用前 `git branch` 只有 `main` 與 `step/00-master-plan`，`git worktree list` 只有主目錄。
- `.claude/settings.json` 的 deny 規則在 Claude Code 信任此資料夾之後才生效。
