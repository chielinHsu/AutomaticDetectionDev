# ADR-030 導入 AI 結構 2.0.0 與範本升級

- 日期：2026-10-21
- 決定者：使用者（要求整套結構統一到 2.0.0），Claude Code (Claude Sonnet 5.5) 執行
- 狀態：採用（補充 [ADR-000](000-ai-agent-structure.md)；ADR-000 的決定仍然有效）

## 背景

AI 共用結構 1.0.0（ADR-000，2026-10-05）把本專案的 `CLAUDE.md` 內容抽成範本。2026-10-21 使用者要求所有專案與工作資料夾照搬本專案實際在用的整套結構，再加兩件事：一、回覆一律繁體中文（含其他 session 開出的子 session 與委派的 subagent），任務完成時要總結；二、非單純需求在實作前先展開成「需求進度表」。範本 `ai-project-template/` 因此升到 2.0.0（內容由另一個 agent 完成，本 ADR 不改動範本）。本專案是 2.0.0 的來源，所以只需補上新增的部分，並把範本管理的檔案換成 2.0.0 版。

## 決定

範本管理的檔案直接換成 2.0.0 版：`.agents/harness/engineering-workflow.md`（新增「需求展開與進度表」）、`.agents/skills/ai-handoff/SKILL.md`（加「更新需求進度表」與「回覆時總結」，鏡像用 `scripts/ai/sync-skills.ps1` 同步）、`scripts/ai/check-ai-structure.ps1`（檢查 2.0.0 的必要檔案、AGENTS 四條關鍵字、需求進度表、`decisions.md` 是索引、模型路由）、`tools/ste-lint.mjs`（認得 `ste-data` 與舊的 `aoi-data`，跳過 `_template`）、`docs/plan/_template.md`、`docs/adr/_template.md`、`.agents/harness/TEMPLATE_VERSION`（2.0.0）。`scripts/ai/sync-skills.ps1` 與範本逐位元相同，沒有更動。

專案擁有的檔案合併，不整份換掉：

- `AGENTS.md`：「不可省略的三條」改為四條（第 1 條加繁中含子 session 與 subagent、完成總結；新增第 4 條需求進度表）；保留本專案加的第 6 項必讀 `docs/architecture.md`。
- `CLAUDE.md`：保留六份匯入與 ADR-019 說明；補上 architect 系列的計畫由主 agent 寫進進度表、Step Protocol 第 7 步用 `start_session`／`spawn_task`、委派時交代繁體中文。
- `.agents/harness/project-rules.md`：Step Protocol、四種輸出、SOLID、Commit 規則（本專案是它們的來源）全部保留；補上四種輸出的適用範圍（小修正免）、Step Protocol 與進度表的連動、編碼、上層共用規則檔（`D:\AOI_Project\Qt\CLAUDE.md`）優先的註記、驗證指令。
- `AI_CONTEXT.md`、`MODEL_ROUTING.md`、`.claude/agents/`：只補新增的段落與「四條」的字眼。`scripts/ai/check-model-routing.mjs`、`bench-model-routing.mjs`、hook 與其 `AOI_*` 環境變數維持專案版本（範本版用 `AI_*`）。
- `docs/steps/_template/` 保留專案版（它是範本版的來源，內容含本專案的情境，id 用 `aoi-data`，lint 兩種都認得）。
- `docs/current-status.md`：現有的進度表升格為「需求進度表」並移到最前面，加「計畫」「驗證」欄；S00–S12 與「—」修正列照舊，新增 R01–R04 回填 2026-10-15 之後的需求；其餘章節原樣保留在表格之後；新增「待使用者決定」。

## 否決的替代方案

- 用 `New-AiProject.ps1 -Refresh` 整批覆蓋：會連專案已調整的檔案一併處理，且不易逐項核對差異；改為逐檔比對後複製範本管理的檔案。
- 把進度表另建新檔：違反單一事實來源，AGENTS 第 4 條與 `check-ai-structure.ps1` 都指向 `docs/current-status.md`。
- 刪掉 `docs/current-status.md` 的歷史章節：內容仍是各步驟的證據，只調整順序，不刪除。

## 後果

- `check-ai-structure.ps1` 對 `docs/current-status.md` 的行數警告上限是 150 行；本檔仍超過，會看到警告（不是失敗）。要消除就把歷史章節移到 `docs/changelog-ai.md` 或步驟資料夾，需要使用者決定。
- 範本升級要重新同步時，只換範本管理的檔案；專案擁有的檔案照本 ADR 的原則合併。
- R04 的提交與合併方式等使用者決定（本專案有 remote `origin`、沒有 CI）。
