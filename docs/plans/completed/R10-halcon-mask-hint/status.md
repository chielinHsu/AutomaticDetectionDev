# R10 狀態

> 快照，不是歷史：只寫「現在」，有實質進度時原地修改（歷史在同資料夾的 `changelog.md`，細項的狀態在 `plan.md` 的細項表）。

- 更新日期：2026-10-30
- 狀態：✅ 完成（R10.1–R10.5）
- 分支：`claude/r09-halcon-mask-hint-f0d064`，2026-10-30 以 `--no-ff` 合併回 `main`（`3ed3832`，本機，沒有 push）

## 目前細項

- 無。計畫完成，資料夾在 `completed/`。

## 阻礙

- 無。

## 待使用者決定

- R07 留下的另外兩件後續還沒開計畫：通用樣本比對與 Socket 結構偵測（R08）並存或整合；右側 dock 重疊。
- worktree `.claude/worktrees/r09-halcon-mask-hint-f0d064` 與分支可以清掉（內容已在 `main`）。

## 接續資訊

- 設計在 [ADR-042](../../../adr/042-match-mask-advice.md)；規則 `src/application/interaction/MatchAdvice`，卡片 `MessageTexts::matchAdviceCard`，Presenter 的 Idle 分支在 `AutoRoiPresenter::publishMessage`。
- 測試台（`tests/ui/PresenterRig.h`）的 `maskhint` back end 要排在 `broken` 之前：A/B 比較的預設配對取倒數兩個可用的（`tst_compare`），錯誤卡片的建議取最後一個可用的（`tst_presenter`）。
- 卡片顯示時提示列被蓋住（訊息列的規則）；按〔關閉〕或 Esc（沒有選取時）提示回來。
- OpenCV 若之後改成遮罩比較好，只要在它的工廠把 `traits.suggestsMatchMask` 打開。
