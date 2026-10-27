---
name: ai-handoff
description: "收工與交接流程。在 Claude Code 與 Codex 之間切換、結束一段有意義的修改、或使用者說「交接」「收工」「更新 handoff」「更新 changelog」「更新 status」「更新進度」時使用。依序更新本次動到的計畫（docs/plans/<狀態>/<ID>-<slug>/ 的 plan.md 細項表、status.md、changelog.md）、計畫層級的狀態改變時更新 docs/plans/INDEX.md、覆寫 .ai/handoff.md、有設計決策時新增 docs/adr/ 與 docs/decisions.md 索引，執行結構檢查，並在回覆時總結修改內容。純問答、沒有改動任何檔案時不需要。"
---

# AI 交接

兩個 agent 看不到彼此的對話，唯一的共同記憶是 repository 內的文件。本 skill 把「做了什麼、做到哪、別動什麼」寫進那些文件。各文件的職責見 `.agents/harness/engineering-workflow.md` 第 0 節；計畫的規則見同檔「計畫分開管理」。**只更新本次動到的計畫**，不要把紀錄寫進其他計畫或專案層級的檔案。

## 收工（交出）

1. 先蒐集事實，不憑記憶寫：`git status`、`git diff --stat`、本次實際執行過的驗證指令與結果、本次動到哪些計畫（`docs/plans/INDEX.md`）。
2. **細項表**（該計畫的 `plan.md`）：本次動到的細項逐列更新狀態（✅ ⏳ ⬜ ⚠️ ⛔）、分支／commit、驗證（`VERIFIED`／`PARTIALLY VERIFIED`／`NOT VERIFIED` 與實際跑了什麼）；完成的列不刪除；做到一半新發現的細項新增為 ⬜。需求還沒有計畫時，先依路由規則建立（從 `docs/plans/_template/` 複製，`INDEX.md` 加一列，編號用 `INDEX.md` 的「下一個可用編號」並把它加 1）。
3. **`status.md`**（同一個計畫）：原地改成「現在」的快照——目前細項、阻礙、待使用者決定、下一步、接續資訊；不重抄細項表、不累積歷史。
4. **`changelog.md`**（同一個計畫）：在**最上方**新增一筆（格式見 `docs/plans/_template/changelog.md`）。動到別的計畫管的設計或介面時，在那個計畫的 `changelog.md` 另記一筆「受影響的內容」，兩邊互相連結。
5. **計畫層級的狀態改變時**（新增、開始、受阻、完成、取消）：改 `plan.md` 的 frontmatter 與 `INDEX.md` 那一列；完成或取消就把資料夾搬到 `completed/` 或 `cancelled/`（用 `git mv`），並更新 `INDEX.md` 的位置與指向它的連結。
6. 有跨計畫的已知問題或待使用者決定時，才改 `docs/current-status.md`（專案層級的快照）。
7. **覆寫** `.ai/handoff.md`：整份重寫，不累積歷史，保持在 60 行內；寫本次動到哪些計畫（連到它們的 `status.md`）、未提交的變更、不要動的東西。
8. 本次有設計決策（選 A 不選 B、改變介面、否決某做法）時，新增 `docs/adr/NNN-slug.md`（範本 `docs/adr/_template.md`，編號遞增），並在 `docs/decisions.md` 的索引**最上方**加一行；決定取代舊決定時，新舊兩筆都標明取代關係。計畫的 `plan.md`「決策」一節連到它。
9. 改過入口檔、skill 或搬動了計畫資料夾時，執行 `powershell -ExecutionPolicy Bypass -File scripts/ai/check-ai-structure.ps1`，有 FAIL 先修掉。
10. **回覆時總結修改內容**（使用繁體中文）：改了哪些檔案與行為、每個計畫的細項狀態變化、實際執行的驗證與結果、未驗證或未完成的事項、下一步建議。

## 開工（接手）

1. 讀 `.ai/handoff.md`、`docs/current-status.md` 與 `docs/plans/INDEX.md`，依「計畫分開管理」的路由規則找出這次要處理的計畫。
2. 只讀那個計畫的 `plan.md` 與 `status.md`；需要歷史時才讀它的 `changelog.md`。3.0.0 之前的紀錄在 `docs/legacy/`，追查舊歷史時才讀。
3. 跑 `git status` 與該分支的 `git log`，確認工作目錄與 `status.md` 一致；不一致時以程式碼與實際的測試結果為準，並在回報中說明差異。
4. 交接單或 `status.md` 列在「不要動」的項目，沒有使用者明確要求就不碰。

## 寫作原則

- 只寫可驗證的事實；沒有執行過的檢查，寫「未驗證」，不寫「應該可以」。
- 檔案用相對路徑；列出受影響的檔案，不貼 diff。
- 工作目錄裡不是自己改的未提交修改，記在交接單的「未提交的變更」並註明來源不明，不要代為整理。
- 署名：changelog 的 agent 欄位寫實際的工具與模型，例如 `Claude Code (模型名稱)` 或 `Codex (模型名稱)`；不要從舊紀錄抄。
- 多個 session 同時進行不同計畫時，各自只改自己的計畫資料夾；`INDEX.md` 與 `.ai/handoff.md` 是共用的，改之前先重新讀一次，只改自己那一列或那一段。
