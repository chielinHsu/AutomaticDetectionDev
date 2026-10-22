---
name: ai-handoff
description: "收工與交接流程。在 Claude Code 與 Codex 之間切換、結束一段有意義的修改、或使用者說「交接」「收工」「更新 handoff」「更新 changelog」「更新 status」「更新進度表」時使用。依序更新 docs/current-status.md 的需求進度表、.ai/handoff.md、docs/changelog-ai.md、docs/adr/ 與 docs/decisions.md 索引，執行結構檢查，並在回覆時總結修改內容。純問答、沒有改動任何檔案時不需要。"
---

# AI 交接

兩個 agent 看不到彼此的對話，唯一的共同記憶是 repository 內的文件。本 skill 把「做了什麼、做到哪、別動什麼」寫進那些文件。各文件的職責見 `.agents/harness/engineering-workflow.md` 第 0 節；需求進度表的規則見同檔「需求展開與進度表」。

## 收工（交出）

1. 先蒐集事實，不憑記憶寫：`git status`、`git diff --stat`、本次實際執行過的驗證指令與結果。
2. **更新需求進度表**（`docs/current-status.md` 開頭）：本次動到的 `Rnn`／`Rnn.m` 逐列更新狀態（✅ ⏳ ⬜ ⚠️ ⛔）、分支／commit、驗證（`VERIFIED`／`PARTIALLY VERIFIED`／`NOT VERIFIED`）；完成的列不刪除；做到一半新發現的細項新增為 ⬜；待使用者決定的事列在表格下方。非單純需求若還沒有列，先補上。
3. 更新 `docs/current-status.md` 其餘段落：把完成的項目移出待辦，新增本次發現的已知問題。
4. **覆寫** `.ai/handoff.md`：整份重寫，不累積歷史，保持在 60 行內。
5. 在 `docs/changelog-ai.md` **最上方**新增一筆，格式見該檔開頭的範例。
6. 本次有設計決策（選 A 不選 B、改變介面、否決某做法）時，新增 `docs/adr/NNN-slug.md`（範本 `docs/adr/_template.md`，編號遞增），並在 `docs/decisions.md` 的索引**最上方**加一行；決定取代舊決定時，新舊兩筆都標明取代關係。
7. 改過入口檔或 skill 時，執行 `powershell -ExecutionPolicy Bypass -File scripts/ai/check-ai-structure.ps1`，有 FAIL 先修掉。
8. **回覆時總結修改內容**（使用繁體中文）：改了哪些檔案與行為、需求進度表各列的狀態變化、實際執行的驗證與結果、未驗證或未完成的事項、下一步建議。

## 開工（接手）

1. 讀 `.ai/handoff.md` 與 `docs/current-status.md`（先看需求進度表，確認自己要接的列與其計畫檔）。
2. 跑 `git status`，確認工作目錄與交接單一致；不一致時以工作目錄為準，並在回報中說明差異。
3. 交接單列在「不要動」的項目，沒有使用者明確要求就不碰。

## 寫作原則

- 只寫可驗證的事實；沒有執行過的檢查，寫「未驗證」，不寫「應該可以」。
- 檔案用相對路徑；列出受影響的檔案，不貼 diff。
- 工作目錄裡不是自己改的未提交修改，記在交接單的「未提交的變更」並註明來源不明，不要代為整理。
- 署名：changelog 的 agent 欄位寫實際的工具與模型，例如 `Claude Code (模型名稱)` 或 `Codex (模型名稱)`；不要從舊紀錄抄。
