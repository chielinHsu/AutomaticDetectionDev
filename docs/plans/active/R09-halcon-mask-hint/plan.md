---
id: R09
title: HALCON 建議用 M 點端子的遮罩當比對區（畫面上的提醒）
type: 功能
status: 進行中（實作完成，待合併）
created: 2026-10-29
updated: 2026-10-29
depends_on: [R07]
related: [ADR-028, ADR-035, ADR-027, ADR-021]
branch: claude/r09-halcon-mask-hint-f0d064
---

# R09 — HALCON 建議用 M 點端子的遮罩當比對區（畫面上的提醒）

> 從 R07 收尾時留給使用者決定的三件後續裡挑出第 (1) 件（[R07 status](../../completed/R07-multi-sample-merge/status.md)）。
> 量測的依據在 [R07 plan 的「R07.8 的量測」](../../completed/R07-multi-sample-merge/plan.md)：HALCON 用整個主體框建範本時召回 855／1068、長槽的鋸齒拿到 0.70；改用 `M` 點端子自動擷取的遮罩之後召回 1059、鋸齒 0.48。OpenCV 用遮罩反而多約 100 個離格的誤報，所以只有 HALCON 要提醒。

## 目標

使用者用 HALCON back end 時，建好樣本但還沒設定比對區（`ComponentType::match` 是預設 ＝ 整個主體框）就要搜尋，畫面上提醒「建議用 `M` 工具點一下端子，用自動擷取的遮罩當比對區」並說明原因；OpenCV 與假 back end 不提醒。提醒不擋搜尋：照樣可以按 F5。

## 範圍

- application 層的規則（沒有 Qt）：哪個 back end 建議遮罩（`Backend` 的特性，不比對名稱字串）、現在該提醒哪些樣本、按過〔關閉〕的不再提醒。
- presentation 只負責顯示：沿用訊息列既有的卡片，文案加進 `docs/ux/copy.json`（zh_TW、en）。
- 單元測試（application 的規則）、整合測試（`tst_presenter`，用測試台裡模擬 HALCON 的假 back end）、`tst_messages` 的文案 key。
- 四種輸出與 `perf.md`（`docs/steps/R09-halcon-mask-hint/`）、ADR-036。

## 不做

- 不改搜尋器、use case、`domain`、Rust；不動 `ContractTolerances.h`。
- 不自動替使用者擷取遮罩（樣本的中心不一定在端子上；點哪裡是使用者的判斷）。
- 不改 OpenCV 的行為（量測顯示 OpenCV 整框比較好）。

## 細項

細項表是這個計畫進度的單一事實來源。新展開的細項先標 ⬜；開始 ⏳；完成 ✅ 並補分支／commit 與驗證；有未解決問題 ⚠️；取消 ⛔ 並寫原因。完成的列不刪除。

| 編號 | 細項 | 狀態 | 分支／commit | 驗證 |
|---|---|---|---|---|
| R09.1 | application：`Backend::traits`（`BackendTraits::suggestsMatchMask`，HALCON 的工廠填 true）；`interaction/MatchAdvice`（該提醒哪些樣本、〔關閉〕過的不再提醒）；`tests/core/tst_match_advice` | ✅ | `claude/r09-halcon-mask-hint-f0d064` | VERIFIED：`tst_match_advice` 9／9、`check-deps` 0 violations |
| R09.2 | presentation：`AutoRoiPresenter` 在 Idle 顯示建議卡片（`MessageTexts::matchAdviceCard`：〔用 M 點端子〕〔關閉〕），特性經 `ISearchEngineSink::useSearchEngine` 交過來；`copy.json` 的文案 | ✅ | 同上 | VERIFIED：`tst_messages`、`tst_presenter` 88 passed（新增 2 項） |
| R09.3 | 測試：`tst_presenter`（測試台多一個建議遮罩的假 back end）、`tst_messages`；完整 `test.ps1`、`check-deps`、`ste-lint` | ✅ | 同上 | PARTIALLY VERIFIED：完整 `test.ps1`（Release＋OpenCV＋HALCON＋Rust，67 項）第一次 64／67、修正測試台後 `-NoBuild` 重跑 65／67；`tst_lattice`、`tst_generate_objects` 在 `main` 已提交的狀態本來就失敗（主 checkout 靠 Codex 未提交的測試修改才通過，見 `status.md`）；`tst_compare` 的 2 項是測試台的順序，修正後重跑通過 |
| R09.4 | 實際執行程式看一次（`--backend halcon`；dongle 讀不到時改用假 back end 說明限制） | ✅ | 同上 | VERIFIED：`AiInspTool.exe --backend halcon --demo 5000`（runtime 授權）框一個樣本後訊息列出現卡片（截圖在 session 的共用筆記資料夾 `r09-halcon-advice-card.png`）；真實影像沒有試（合成板子已足以驗證觸發） |
| R09.5 | 四種輸出與 `perf.md`、ADR-036、計畫文件、交接單；提交到分支（不 push、不合併） | ✅ | 同上（commit 見 `changelog.md`） | `ste-lint` 0 errors；合併回 `main` 待使用者決定 |

圖例：✅ 完成　⏳ 進行中　⬜ 尚未開始　⚠️ 有未解決問題　⛔ 取消

## 決策

- 設計與否決的做法在 [ADR-036](../../../adr/036-match-mask-advice.md)：
  - 哪個 back end 建議遮罩是 back end 自己的特性（`Backend::traits`），由 adapter 的工廠填；presentation 與規則都不比對 `"halcon"` 這種字串。
  - 規則在 application 的 `MatchAdvice`（ADR-011 的做法：沒有 Qt、決定性的單元測試）；Presenter 只在 `Idle` 時把結果變成卡片。
  - 顯示用訊息列的**卡片**（`IMessageView::showCard`，`AutoRoiPresenter` 擁有），不用提醒卡片（`showNotice` 歸 `BackendPresenter`，HALCON 的 runtime 授權提醒會和它搶同一個位置）。
  - 觸發時機：只要回到 `Idle` 而且條件成立就顯示（建完樣本、切到 HALCON、接受或捨棄之後都算），同一個樣本按過〔關閉〕就不再提醒；設了比對區自然不再符合條件。

## 完成定義

- [ ] 建置通過、`scripts/dev/test.ps1` 預設建置全部通過（原本就有的 2 個 XFAIL 與 8 個有跳過項目的測試除外）
- [ ] 四種輸出與 `perf.md` 已交付（`docs/steps/R09-halcon-mask-hint/`），`node tools/ste-lint.mjs` 無 error
- [ ] 細項表全部 ✅ 或 ⛔；`status.md`、`changelog.md` 已更新
- [ ] 提交在分支上；合併回 `main` 由使用者決定（之後再搬到 `completed/`）

## Commit 範本

格式一律依 `project-rules.md`「Commit 訊息」；這裡只列內容參考。

```
[Caspar]feat: HALCON 建好樣本後在訊息列建議用 M 點端子的遮罩

R07.8 的量測顯示 HALCON 用整個主體框建範本時…改成…

測試 :
…

Co-Authored-By: Claude <模型名稱>

[Test]
Build OK, Test OK
```

## Start prompt（開新 session 時貼上）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續專案。回覆一律使用繁體中文（zh-TW），完成時總結修改內容。
請先讀 AGENTS.md、docs/plans/INDEX.md，以及計畫 docs/plans/active/R09-halcon-mask-hint/ 的 plan.md 與 status.md，
只做 R09 的下一個細項。完成後更新該計畫的細項表、status.md、changelog.md（ai-handoff skill），
計畫層級的狀態改變時才改 INDEX.md，並依 project-rules.md 的 Git 規則提交。
```
