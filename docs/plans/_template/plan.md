---
id: Rnn
title: TODO 計畫名稱
type: TODO 功能／修正／重構／文件／雜項
status: 規劃中
created: YYYY-MM-DD
updated: YYYY-MM-DD
depends_on: []
related: []
branch: TODO（例如 feat/slug、step/NN-slug）
---

# Rnn — TODO 計畫名稱

> 範本（AI 結構 3.0.0）：複製整個 `docs/plans/_template/` 成 `docs/plans/active/<ID>-<slug>/`，填好本檔，並在 `docs/plans/INDEX.md` 加一列。
> 角括號與 `TODO` 是要填的欄位；用不到的段落刪除。規則見 `.agents/harness/engineering-workflow.md`「計畫分開管理」。
> frontmatter 的 `status` 和 `INDEX.md` 的狀態一致：規劃中、進行中、受阻、完成、取消。

## 目標

TODO：一到三句話，這個計畫完成後使用者能做什麼。

## 範圍

- TODO：要做的事。

## 不做

- TODO：明確不做的事（避免順手擴張）。

## 細項

細項表是這個計畫進度的單一事實來源。新展開的細項先標 ⬜；開始 ⏳；完成 ✅ 並補分支／commit 與驗證；有未解決問題 ⚠️；取消 ⛔ 並寫原因。完成的列不刪除。細項需要寫實作細節時，另寫 `tasks/<ID>.<m>-<slug>.md` 並連到它。

| 編號 | 細項 | 狀態 | 分支／commit | 驗證 |
|---|---|---|---|---|
| Rnn.1 | TODO | ⬜ | | |
| Rnn.2 | TODO | ⬜ | | |

圖例：✅ 完成　⏳ 進行中　⬜ 尚未開始　⚠️ 有未解決問題　⛔ 取消

## 決策

- TODO：本計畫採用的做法與理由；跨計畫或改變架構的決策寫成 `docs/adr/NNN-slug.md`，這裡只放連結。

## 完成定義

- [ ] 建置通過、測試通過（列出測試名稱或指令，見 `AI_CONTEXT.md`「建置與測試」）
- [ ] 類型是「功能」或 Step Protocol 的步驟：四種輸出與 `perf.md` 已交付（`project-rules.md`）
- [ ] 細項表全部 ✅ 或 ⛔；`status.md`、`changelog.md` 已更新（`ai-handoff` skill）
- [ ] 完成後資料夾搬到 `docs/plans/completed/`，`INDEX.md` 已更新
- [ ] TODO：本計畫專屬的檢查

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
你正在 <專案資料夾> 繼續專案。回覆一律使用繁體中文（zh-TW），完成時總結修改內容。
請先讀 AGENTS.md、docs/plans/INDEX.md，以及計畫 docs/plans/active/<ID>-<slug>/ 的 plan.md 與 status.md，
只做 <ID>（或細項 <ID>.<m>）。完成後更新該計畫的細項表、status.md、changelog.md（ai-handoff skill），
計畫層級的狀態改變時才改 INDEX.md，並依 project-rules.md 的 Git 規則提交。
```
