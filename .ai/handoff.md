# Agent Handoff

- 最後一位 agent：Claude Code (Claude Sonnet 5.5)，由主 session 委派的 developer-deep 執行
- 日期：2026-10-21
- 分支：`docs/ai-structure-v2`（git worktree，基於 `codex/static-repeatability` b621d30）；本次修改已本機提交，留在此分支，未推送、未合併；合併與推送由使用者決定。

## 本次交付：導入 AI 結構 2.0.0（[ADR-030](../docs/adr/030-ai-structure-2-0-0.md)，需求進度表 R04）

- 範本管理檔換成範本 2.0.0 版：`.agents/harness/engineering-workflow.md`、`.agents/skills/ai-handoff/SKILL.md`（鏡像已用 `sync-skills.ps1` 同步）、`scripts/ai/check-ai-structure.ps1`、`tools/ste-lint.mjs`、`.agents/harness/TEMPLATE_VERSION`（2.0.0）；新增 `docs/plan/_template.md`、`docs/adr/_template.md`。`scripts/ai/sync-skills.ps1` 與範本相同，沒有更動。
- 專案擁有檔合併：`AGENTS.md` 改為四條、`CLAUDE.md`、`.agents/harness/project-rules.md`（補四種輸出適用範圍、進度表連動、編碼、上層規則優先、驗證指令）、`AI_CONTEXT.md`（Context Routing 加進度表一列）、`MODEL_ROUTING.md` 與 `.claude/agents/`（四條、architect 計畫由主 agent 寫進進度表）。
- `docs/current-status.md`：現有進度表升格為「需求進度表」並移到最前面，加「計畫」「驗證」欄，新增 R01–R04 與「待使用者決定」；其餘章節原樣保留在表格後。
- ADR-030 與 `docs/decisions.md` 索引一行；`docs/changelog-ai.md` 新增一筆。

## 實際驗證（在 worktree 根目錄）

- `scripts/ai/sync-skills.ps1 -Check`：OK（1 mirrored）。
- `scripts/ai/check-ai-structure.ps1 -Strict`：RESULT OK，0 FAIL、1 WARN（`docs/current-status.md` 198 行，超過範本上限 150 行；內容未刪，要不要把歷史章節移到 changelog 等使用者決定）。
- `node scripts/ai/check-model-routing.mjs`：68 passed、0 warning。
- `node tools/ste-lint.mjs`：984 sentences、0 errors、0 warnings（與升級前相同）。
- 沒有建置、沒有跑 `scripts/dev/test.ps1`：本次不改程式、測試或建置設定。

## 不要動與界線

- `ai-project-template/` 已升級成 2.0.0（New-AiProject.ps1、README、template 全套），與本次一起提交。
- `scripts/ai/check-model-routing.mjs`、`bench-model-routing.mjs`、`.claude/hooks/model_router.mjs` 與 `AOI_*` 環境變數維持專案版本（範本版用 `AI_*`）；`docs/steps/_template/` 保留專案版。
- 不改程式原始碼、測試、perf 資料。靜態重複性（ADR-029）、S12 的狀態沒有變動。

## 還沒確認

- 上層規則 `D:\AOI_Project\Qt\CLAUDE.md` 的完成後流程含推送與開 PR；本專案 `project-rules.md` 對 push 的規定較具體，已註明該流程不適用、推送與開 PR 要使用者當次要求。
- Codex 端沒有實際跑過 2.0.0 的流程；`check-ai-structure.ps1` 只檢查結構。

## 先前仍有效的提醒（靜態重複性，2026-10-16）

- 建置：沙箱 `env.ps1` 曾選到 MSVC 19.00；要用本機權限並前置 `VCToolsInstallDir/bin/Hostx64/x64`（確認 19.44）。source-side 的舊 `.moc` 會污染 qmake 依賴，已備份在 `build/repeatability-cache-backup`，不要回填。
- 真實資料測試：`AOI_REPEATABILITY_DATA` 指向 `D:/Benano/20261014_20261014_4677_AdvancePin_Proj`；`AOI_REPEATABILITY_SCREENSHOTS` 設輸出目錄可重產畫面。
- 效能預算與基準線要等使用者指示才重建；Debug 的 Presenter 2 秒忙碌等待案例是既有的不穩定測試。
- 其他待決定事項見 `docs/current-status.md` 的「待使用者決定」。
