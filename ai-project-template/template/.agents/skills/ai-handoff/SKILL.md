---
name: ai-handoff
description: "收工與交接流程。在 Claude Code 與 Codex 之間切換、結束一段有意義的修改、或使用者說「交接」「收工」「更新 handoff」「更新 changelog」「更新 status」時使用。依序更新 .ai/handoff.md、docs/changelog-ai.md、docs/current-status.md、docs/decisions.md，並執行結構檢查。純問答、沒有改動任何檔案時不需要。"
---

# AI 交接

兩個 agent 看不到彼此的對話，唯一的共同記憶是 repository 內的文件。本 skill 把「做了什麼、做到哪、別動什麼」寫進那些文件。各文件的職責見 `.agents/harness/engineering-workflow.md` 第 0 節。

## 收工（交出）

1. 先蒐集事實，不憑記憶寫：`git status`、`git diff --stat`、本次實際執行過的驗證指令與結果。
2. **覆寫** `.ai/handoff.md`：整份重寫，不累積歷史，保持在 60 行內。
3. 在 `docs/changelog-ai.md` **最上方**新增一筆，格式見該檔開頭的範例。
4. 更新 `docs/current-status.md`：把完成的項目移出待辦，新增本次發現的已知問題。
5. 本次有設計決策（選 A 不選 B、改變介面、否決某做法）時，在 `docs/decisions.md` **最上方**新增一筆，寫明理由與否決的替代方案。決定取代舊決定時，標明取代哪一筆。
6. 改過入口檔或 skill 時，執行 `powershell -ExecutionPolicy Bypass -File scripts/ai/check-ai-structure.ps1`，有 FAIL 先修掉。

## 開工（接手）

1. 讀 `.ai/handoff.md` 與 `docs/current-status.md`。
2. 跑 `git status`，確認工作目錄與交接單一致；不一致時以工作目錄為準，並在回報中說明差異。
3. 交接單列在「不要動」的項目，沒有使用者明確要求就不碰。

## 寫作原則

- 只寫可驗證的事實；沒有執行過的檢查，寫「未驗證」，不寫「應該可以」。
- 檔案用相對路徑；列出受影響的檔案，不貼 diff。
- 工作目錄裡不是自己改的未提交修改，記在交接單的「未提交的變更」並註明來源不明，不要代為整理。
- 署名：changelog 的 agent 欄位寫實際的工具與模型，例如 `Claude Code (模型名稱)` 或 `Codex (模型名稱)`；不要從舊紀錄抄。
