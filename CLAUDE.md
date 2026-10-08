# Claude Code 專案入口

@AGENTS.md
@AI_CONTEXT.md
@.agents/harness/engineering-workflow.md
@.agents/harness/project-rules.md
@docs/architecture.md
@MODEL_ROUTING.md

上面六份每個 session 都會載入（`docs/architecture.md` 是本專案額外加的：分層規則原本就在 CLAUDE.md，每個 session 都看得到；`MODEL_ROUTING.md` 是 Claude 專用的模型路由規則，2026-10-08 加入，見 ADR-019）。`.ai/handoff.md` 與 `docs/current-status.md` 會頻繁變動，不自動匯入；非單純任務開始前，依 `AGENTS.md` 的清單讀取。

本檔只放 Claude 專屬規則，共通內容在上面的文件，不要在此複製。本專案原本寫在這裡的環境、分層規則、Step Protocol 與 Commit 規則，已於 2026-10-05 搬到共用文件（見 `docs/adr/000-ai-agent-structure.md`）。

## Claude 專屬規則

- **Skill**：`.claude/skills/` 是 `.agents/skills/` 的鏡像，改 skill 時改 `.agents/skills/` 的來源，再執行 `scripts/ai/sync-skills.ps1`。只給 Claude 用的 skill 要登記在 `.agents/harness/skill-sync.json` 的 `claudeOnly`。
- **權限與 hook**：`.claude/settings.json` 控制工具權限。設定允許某個指令，不代表已授權 commit、push、刪除或對外發布。
- **流程圖**：`engineering-workflow.md` 要求的修改前後流程圖，在對話中優先用可用的視覺化工具渲染成圖；工具不可用時退回 Mermaid，並說明原因。寫進文件的流程圖一律保留 Mermaid 原始碼。
- **模型路由**：每個需求都主動評估要不要委派 subagent（`MODEL_ROUTING.md`）：一般任務 Sonnet（`developer`）、複雜或高風險 Opus（`architect`，唯讀）、簡單低風險 Haiku（`fast-worker`，唯讀）；沒把握用 Sonnet。發現安全風險或兩次有效除錯失敗時重新評估，必要時升 Opus。`UserPromptSubmit` hook（`.claude/hooks/model_router.mjs`）的提示只是關鍵字分類，要依實際任務判斷；不要說主 session 的模型已經切換。改過這些檔案後跑 `node scripts/ai/check-model-routing.mjs`。
- **Step Protocol 第 7 步**：用 `spawn_task` 為下一步開新 session。
- **個人偏好**放 `CLAUDE.local.md`（已在 `.gitignore`），不要寫進本檔。
