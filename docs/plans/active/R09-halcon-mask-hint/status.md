# R09 狀態

> 快照，不是歷史：只寫「現在」，有實質進度時原地修改（歷史在同資料夾的 `changelog.md`，細項的狀態在 `plan.md` 的細項表）。

- 更新日期：2026-10-29
- 狀態：進行中（R09.1–R09.5 完成並提交在分支上；合併回 `main` 待使用者決定，合併後搬到 `completed/`）
- 分支：`claude/r09-halcon-mask-hint-f0d064`（worktree `.claude/worktrees/r09-halcon-mask-hint-f0d064`，從 `main` 的 `37bd396` 切出；沒有 push）

## 目前細項

- 無。實作、測試、文件都完成；等使用者決定合併。

## 阻礙

- 無。

## 待使用者決定

- 要不要把本分支合併回 `main`（建議 `--no-ff`），合併後把本資料夾搬到 `docs/plans/completed/`。
- 完整 `test.ps1` 在本 worktree（`main` 已提交的狀態＋本分支）有 2 個和本計畫無關的失敗：`tst_lattice`（`aStaggeredBlockPredictsExactlyItsMissingCells`、`aStaggeredBlockWithHalfMissingStillFitsOneLattice`）與 `tst_generate_objects`（`aWeakPartInARowNeedsBothNeighbours`）。主 checkout 的 `tests/domain/tst_lattice.cpp`、`tests/core/tst_generate_objects.cpp` 有 Codex 未提交的修改（R07.5–R07.7 的回歸修復），那邊通過；要讓 `main` 自己通過，得把那兩個測試檔的修改提交。

## 下一步

1. 使用者檢視分支與截圖（共用筆記資料夾的 `r09-halcon-advice-card.png`）。
2. 合併後：資料夾搬到 `completed/`、`INDEX.md` 改 ✅。

## 接續資訊

- 設計在 [ADR-036](../../../adr/036-match-mask-advice.md)；規則 `src/application/interaction/MatchAdvice`，卡片 `MessageTexts::matchAdviceCard`，Presenter 的 Idle 分支在 `AutoRoiPresenter::publishMessage`。
- 測試台（`tests/ui/PresenterRig.h`）的 `maskhint` back end 要排在 `broken` 之前：A/B 比較的預設配對取倒數兩個可用的（`tst_compare`），錯誤卡片的建議取最後一個可用的（`tst_presenter`）。
- 卡片顯示時提示列被蓋住（訊息列的規則）；按〔關閉〕或 Esc（沒有選取時）提示回來。建完樣本的吐司也含「F5」，測試不能直接比對提示的全文。
- 主 checkout 有 Codex 的 R08 未提交修改，本 worktree 看不到；不要去動主 checkout。
