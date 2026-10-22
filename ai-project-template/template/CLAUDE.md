# Claude Code 專案入口

@AGENTS.md
@AI_CONTEXT.md
@.agents/harness/engineering-workflow.md
@.agents/harness/project-rules.md
@MODEL_ROUTING.md

上面五份每個 session 都會載入（`MODEL_ROUTING.md` 是 Claude 專用的模型路由規則）。`.ai/handoff.md` 與 `docs/current-status.md` 會頻繁變動，不自動匯入；非單純任務開始前，依 `AGENTS.md` 的清單讀取。

本檔只放 Claude 專屬規則，共通內容在上面的文件，不要在此複製。

## Claude 專屬規則

- **Skill**：`.claude/skills/` 是 `.agents/skills/` 的鏡像，改 skill 時改 `.agents/skills/` 的來源，再執行 `scripts/ai/sync-skills.ps1`。只給 Claude 用的 skill 要登記在 `.agents/harness/skill-sync.json` 的 `claudeOnly`。
- **權限與 hook**：`.claude/settings.json` 控制工具權限。設定允許某個指令，不代表已授權 commit、push、刪除或對外發布。
- **流程圖**：`engineering-workflow.md` 要求的修改前後流程圖，在對話中優先用可用的視覺化工具渲染成圖；工具不可用時退回 Mermaid，並說明原因。寫進文件的流程圖一律保留 Mermaid 原始碼。
- **模型與推理強度路由**：每個需求都主動評分（複雜度、風險、推理深度、不確定性、範圍、驗證成本），評估要不要委派 subagent（`MODEL_ROUTING.md`）：查詢 `fast-worker`（haiku/low）、一般 `developer`（sonnet/medium）、難但風險不高 `developer-deep`（sonnet/high）、架構安全高風險 `architect`（opus/high）、高風險又根因不明 `architect-deep`（opus/xhigh）；沒把握用 `developer`。先提高推理強度再換模型；兩次有效失敗升一級，解決後降級，最高一級仍失敗就停下來問使用者。`UserPromptSubmit` hook（`.claude/hooks/model_router.mjs`）的評分只是關鍵字的建議，要依實際任務判斷；不要說主 session 的模型或 effort 已經切換。改過這些檔案後跑 `node scripts/ai/check-model-routing.mjs`。委派時一併交代「回覆使用繁體中文（zh-TW）」（`AGENTS.md` 第 1 條）。
- **architect／architect-deep 的計畫**：這兩個 subagent 唯讀、只出計畫；由**主 agent** 把回傳的計畫寫進 `docs/current-status.md` 的需求進度表與 `docs/plan/<ID>-<slug>.md`（`engineering-workflow.md`「需求展開與進度表」），再交給 developer 系列或自己實作。
- **Step Protocol 第 7 步**：用 `start_session` 或 `spawn_task`（工具可用時）為下一步開新 session；prompt 取自下一步 plan 檔底部的「Start prompt」，並提醒新 session 回覆使用繁體中文。
- **個人偏好**放 `CLAUDE.local.md`（已在 `.gitignore`），不要寫進本檔。
