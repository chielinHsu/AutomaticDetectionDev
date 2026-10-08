# ADR-000 AI agent 共用結構（Codex 與 Claude Code）

- 日期：2026-10-05
- 決定者：使用者（要求套用），Claude Code (Claude Sonnet 5.5) 執行
- 狀態：採用

## 背景

專案會在 Claude Code 與 Codex 之間切換。原本只有 `CLAUDE.md`（Claude 專用），專案規則、環境、分層規則與 Step Protocol 都寫在裡面，Codex 看不到。兩邊各維護完整規則會漂移。

## 決定

套用 `ai-project-template/` 的共用結構：

- `AGENTS.md` 是平台中立的入口；`CLAUDE.md` 用 `@AGENTS.md` 匯入，並只放 Claude 專屬規則。
- 專案事實放 `AI_CONTEXT.md`；專案慣例放 `.agents/harness/project-rules.md`；分層與依賴規則放 `docs/architecture.md`。
- 交接用 `.ai/handoff.md`（覆寫）與 `docs/changelog-ai.md`（只新增）。
- 原 `docs/STATUS.md` 更名為 `docs/current-status.md`，所有引用同步更新。
- 決策維持一檔一檔的 `docs/adr/`，`docs/decisions.md` 作為索引。

`CLAUDE.md` 原內容的去向（文字照搬；Step Protocol 有少數步驟調整，細節見 `project-rules.md` 開頭）：

| 原 `CLAUDE.md` 章節 | 新位置 |
|---|---|
| 開頭簡介、環境表 | `AI_CONTEXT.md` |
| 分層與依賴規則 | `docs/architecture.md` |
| SOLID 檢查清單、四種輸出、Step Protocol、Commit 規則 | `.agents/harness/project-rules.md` |
| 不要做的事 | `AI_CONTEXT.md`「限制與不變條件」 |

舊版 `CLAUDE.md` 在 git 歷史中（commit `28414b1`）。

## 否決的替代方案

- 保留 `CLAUDE.md` 全文，只加 `AGENTS.md` 指向它：Codex 要讀一份標著 Claude 專用的檔案，且兩個平台的專屬規則無處放。
- 保留 `docs/STATUS.md` 並讓範本支援自訂檔名：每個專案多一份設定，所有 skill 與腳本都要處理別名。本專案的引用約 30 處，一次改完成本較低。

## 後果

- 12 份 `docs/plan/S*.md` 的 Start prompt、`00-master-plan.md`、`S00-overview/spec.md` 的引用已改成新路徑。
- Step Protocol 第 4 步多了「更新交接單與 changelog」。
- 後續（2026-10-05）：commit 訊息格式由原本的 Conventional Commits 改為使用者的 `[作者]屬性:` 格式，見 `project-rules.md`「Commit 訊息」；Step Protocol 第 5 步同步調整。
- 分層規則原本每個 Claude session 都會載入（在 `CLAUDE.md`）。搬到 `docs/architecture.md` 後，為了維持這個行為，`CLAUDE.md` 額外匯入它，`AGENTS.md` 也把它列入必讀。這是本專案相對於範本預設的唯一增加。
- Codex 端沒有在本機端到端測過（該機器的 Codex CLI 無法執行 `codex exec`）；Claude Code 端已實測匯入有載入。
- 後續（2026-10-08）：新增 Claude 專用的模型路由（[ADR-019](019-model-routing.md)）：根目錄的 `MODEL_ROUTING.md` 由 `CLAUDE.md` 匯入（第六份），另有 `.claude/agents/` 的三個 subagent、`.claude/hooks/` 的 `UserPromptSubmit` hook、`scripts/ai/check-model-routing.mjs`。`AGENTS.md` 的必讀清單與 `check-ai-structure.ps1` 沒有改，Codex 不讀；`ai-project-template/` 沒有改。
