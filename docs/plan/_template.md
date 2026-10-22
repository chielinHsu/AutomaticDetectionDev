# <ID> — <步驟名稱>

> 範本：複製成 `docs/plan/<ID>-<slug>.md`，在 `docs/current-status.md` 需求進度表的「計畫」欄連到它。跨多個 session 或多個步驟的需求才需要計畫檔；小需求只寫在進度表。
> 角括號與 `TODO` 是要填的欄位；用不到的段落刪除。

| 項目 | 內容 |
|---|---|
| 層級 | TODO（例如 介面／系統／底層／驗證／發行） |
| 分支 | `step/NN-slug` |
| 依賴 | TODO（前置的步驟或需求） |
| 進度表編號 | TODO（例如 R03，細項 R03.1 起） |
| 產出位置 | TODO（程式碼資料夾與 `docs/steps/<ID>-<slug>/`） |

> **修訂紀錄**：前面的步驟做完後，若有影響本步驟的新事實，在這裡用引言區塊記下日期與提醒；新的放最上方。

<!--STE-->
## Purpose
(What does this step build? Two to four short sentences.)

## Reason
(Why does the project need it now? What problem does it solve?)

## Effect
(What can the user or the developer do after this step?)
<!--/STE-->

**繁中**：用一兩句話說明這一步做什麼、為什麼、完成後有什麼效果。

## 任務

1. TODO：具體、可驗收的工作項目；每項對應進度表的一個細項（`Rnn.m`）。
2. TODO

## SOLID 重點

TODO：本步驟最相關的一兩條原則，以及怎麼落實。

## 效能檢查（寫入 `perf.md`）

TODO：要量測的指標與預算；沒有效能指標就寫「本步驟無效能指標」。

## 四種輸出（本步驟具體內容）

適用範圍見 `project-rules.md`；小修正免。

| # | 輸出 | 內容 |
|---|---|---|
| 1 | spec.md | TODO |
| 2 | diagram.md | TODO |
| 3 | explainer.html | TODO |
| 4 | storyboard.md | TODO |

## 完成定義

- [ ] 建置通過、測試通過（列出測試名稱或指令）
- [ ] 四種輸出與 `perf.md` 已交付，`node tools/ste-lint.mjs` 無 error
- [ ] `docs/current-status.md` 需求進度表對應列已更新（狀態、分支／commit、驗證）
- [ ] 交接單與 `docs/changelog-ai.md` 已更新（`ai-handoff` skill）
- [ ] TODO：本步驟專屬的檢查

## Commit 範本

格式一律依 `project-rules.md`「Commit 訊息」；這裡只列內容參考。

```
[作者]feat: 一行說明

正文：為什麼改、怎麼改（散文）。

測試 :
實際跑過的檢查與結果。

Co-Authored-By: Claude <模型名稱>

[Test]
Build OK, Test OK
```

## Start prompt（開新 session 時貼上）

```
你正在 <專案資料夾> 繼續專案。請先讀 AGENTS.md、docs/current-status.md、docs/plan/<ID>-<slug>.md，
再依 .agents/harness/project-rules.md 的 Step Protocol 完成步驟 <ID>（分支 step/NN-slug）。只做 <ID>。
回覆一律使用繁體中文（zh-TW），完成時總結修改內容。完成後：commit、合併到預設分支、打 tag step-NN-done、
更新 docs/current-status.md 與交接單（ai-handoff skill）、對我提示 commit 內容。
```
