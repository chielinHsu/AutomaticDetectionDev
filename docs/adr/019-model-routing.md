# ADR-019 模型路由：MODEL_ROUTING.md、三個 subagent、UserPromptSubmit hook

- 日期：2026-10-08
- 決定者：使用者 2026-10-08 要求建立「Intelligent Model Routing System」（ChatGPT 討論裡的設定 prompt）：`MODEL_ROUTING.md`、`.claude/agents/` 的三個 subagent（`fast-worker`／`developer`／`architect`，`haiku`／`sonnet`／`opus` 別名）、`UserPromptSubmit` hook、`settings.json` 保留既有設定、`CLAUDE.md` 引用、驗證流程。分級的判準、hook 改用 Node、兩個 subagent 唯讀、驗證腳本是 Claude Code（Claude Sonnet 5.5 開始，Claude Opus 5.5 完成）的提案並實作。
- 狀態：採用（Claude Code 專用；Codex 沒有對應機制）

## 背景

使用者在 Claude Code 開發多個專案，希望依任務的難度與風險自動用 Haiku、Sonnet 或 Opus，不必每次手動換模型。限制（官方文件，經 `claude-code-guide` 查證）：

- hook 不能切換主 session 的模型；`UserPromptSubmit` 只能經 `additionalContext` 注入提示，結束碼 2 會擋住需求、其他非 0 會顯示 hook error。
- subagent 的定義檔（`.claude/agents/*.md`）可以用 `model` 指定別名；Claude 依 `description` 決定是否委派；subagent 會載入專案的 `CLAUDE.md`。

## 決定

1. **規則**：根目錄的 `MODEL_ROUTING.md`，由 `CLAUDE.md` 匯入（每個 session 都載入）。判準是可檢查的條件（不用 1–5 分）；例子用本專案的模組與 ADR；與 `engineering-workflow.md` 已有的驗證、回報規則不重複。
2. **三個 subagent**：`fast-worker`（`haiku`，唯讀：`tools` 只列 Read、Grep、Glob、Bash、PowerShell）、`developer`（`sonnet`，繼承全部工具但 `disallowedTools: Agent`）、`architect`（`opus`，唯讀，輸出根因、設計與修改計畫，實作交給 `developer` 或主 session）。三個都不能再委派：升級由主 session 決定，避免額度失控。
3. **hook**：`.claude/hooks/model_router.mjs`（Node），`settings.json` 的 `UserPromptSubmit` 用 shell form `node "${CLAUDE_PROJECT_DIR}/.claude/hooks/model_router.mjs"`（Windows 由 Git Bash 執行），timeout 10 秒。關鍵字分類：查詢類（而且沒有修改、分析的字眼）→ Haiku；命中高風險主題 → Opus；其餘 → Sonnet。提示寫明「不是判斷、要重新評估、主 session 的模型沒有切換」。任何失敗都以結束碼 0、不輸出收場；太短的回覆、斜線指令不處理；`AOI_MODEL_ROUTER=off` 可以關掉。
4. **驗證**：`node scripts/ai/check-model-routing.mjs`（subagent 的定義、`settings.json` 與 `git HEAD` 相比沒有少設定、hook 的分類與失敗時的行為、用 Git Bash 跑實際的指令字串、`CLAUDE.md` 的匯入）。Claude Code 是否真的載入，用 headless 的 `claude -p … --output-format stream-json` 從紀錄讀（`MODEL_ROUTING.md` 第 8 節）。

## 否決的替代方案

- **hook 用 Python（設定 prompt 的寫法）**：這台機器的 `python.exe` 只是 Store 捷徑（`python --version` 沒有輸出）；Node v22 是本專案已有的工具鏈（`tools/*.mjs`）。
- **規則放 `.claude/model-routing.md`**（本 session 的第一版）：使用者的 prompt 指定 `MODEL_ROUTING.md`；放根目錄和 `AGENTS.md`、`CLAUDE.md` 並列，比較好找。
- **只寫規則、不加 hook 與 subagent**（第一版）：使用者接著要求自動委派。
- **hook 直接決定、強制委派**：做不到（hook 只能注入提示）；關鍵字分類不理解程式碼，強制反而會在誤判時浪費額度。
- **`developer` 也唯讀或只列固定工具**：它要實作；列固定清單容易漏掉需要的工具。只拿掉 Agent。
- **subagent 用 `omitClaudeMd` 省掉專案文件的 context**：subagent 會因此不知道專案規則（三條不可省略的規則、分層、驗證指令）。實測的成本見下面，要省再評估。

## 後果與驗證

- `check-model-routing.mjs`：44 項通過；在 scratchpad 的複本故意改壞（model 寫版本號、唯讀的加 Edit、拿掉 `disallowedTools`、刪一條權限、`bypassPermissions`、`PowerShell(*)`、hook 的 Opus 分支失效、`CLAUDE.md` 不匯入），每一項都報 FAIL。
- headless 實測（Claude Code 2.1.292）：hook 有執行；`system/init` 列出三個 subagent；三個同時委派、回報回到主 session；實際模型 `fast-worker` → `claude-haiku-4-5-20251001`、`developer` → `claude-sonnet-5-5`、`architect` → `claude-opus-5-5`。**`haiku` 別名在這個版本對應 Haiku 4.5，不是 5.5**；照使用者的要求仍用別名。
- 成本：每個 subagent 啟動時載入 `CLAUDE.md` 與匯入的文件（48K–68K tokens）；只回一句話的委派 Sonnet 約 US$0.17、Opus 約 US$0.24（API 計價）。委派不是免費的，所以規則寫「只在值得時委派」。
- hook 每次送出需求都會執行（約 0.1–0.3 秒，含 Node 與 Git Bash 啟動）；沒有 Git Bash 的電腦，Claude Code 改用 PowerShell 執行 hook，這種情況沒有驗證。
- 互動式 session（桌面 app 的 Code 分頁）：在進行中的 session 改完 `settings.json` 之後，下一則使用者訊息的 context 就出現路由提示，不必重新開 session（官方說檔案監看通常會自動套用，實測相符）。
