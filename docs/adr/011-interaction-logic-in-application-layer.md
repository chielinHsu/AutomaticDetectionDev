# ADR-011 互動規則放在 application 層（沒有 Qt 的 `InspectionSession`），Presenter 只做 Qt 這一側

- 日期：2026-10-06
- 決定者：Claude Code (Claude Sonnet 5.5) 提出並實作（S04）。計畫（`docs/plan/S04-presenter.md`）要求「狀態機不含 Qt」「`InspectionDocument` 在 application 層」；把整套互動規則收進同一個沒有 Qt 的類別，是實作時的決定。使用者沒有針對這份 ADR 另外表示意見。
- 狀態：採用

## 背景

S04 要把畫面接上核心。ux-spec 第 12.4 節把責任分給六個 Presenter（`AutoRoiPresenter`、`ComponentTreePresenter`、`ParamsPresenter`、`LibraryPresenter`、`TypeEditorPresenter`、`HistoryPresenter`）加上 application 層的 `InteractionStateMachine`、`InspectionDocument`、`ProposalSession`、`TypeEditSession`、`UndoStack`。

「按 Enter 之後什麼會發生」不只是狀態機的一格轉移：它同時是提案變成元件、指派名稱與編號、清掉提案堆疊殘留的重做、在文件堆疊上放一個帶著提案的 Command、狀態回到 Idle、選取清空。「Ctrl+Z 作用在哪個堆疊」「刪除作用在提案還是文件」「門檻變了舊的『接受』還能不能重做」也是這種跨物件的規則。這些如果分散在多個 Qt 的 Presenter 裡，每一條都要開視窗、發事件才測得到。

## 決定

1. **規則放在 `application/interaction/InspectionSession`**（沒有 Qt，只用標準庫）：它擁有 `InteractionStateMachine`、`InspectionDocument`、目前的 `ProposalSession`、選取與上次搜尋的區域；所有「事件 → 狀態與資料」的規則都是它的方法。`tst_session` 逐項驗證（含 ADR-002 規則 4、5）。
2. **`AutoRoiPresenter` 只負責 Qt 這一側**：把畫布與 View 的事件交給 session、處理 worker ↔ UI 執行緒（`JobRunner`、`UiPoster`、世代）、忙碌與進度計時，以及把 session 的結果（`buildScene`）發布到各個 View 介面。
3. **Command 與 memento 不可變**（`UndoStack<Target>` 的 `Command::redo/undo` 是 `const`；合併回傳新 Command）。`ProposalSession` 因此是便宜的值型別，可以被整份放進文件堆疊的 Accept／Discard Command，復原時原樣還給預覽。
4. **只做兩個 Presenter**：`ComponentTreePresenter`（選取、刪除）與 `HistoryPresenter`（復原、重做）的責任在 session 裡各只有幾行，併進 `AutoRoiPresenter`；`ParamsPresenter` 獨立（防抖的計時是它自己的責任）。`LibraryPresenter`、`TypeEditorPresenter` 不在 S04 的任務清單。
5. **畫布與物件樹共用一個整數編號**；提案物件加上 `kProposalIdBase`（1 000 000），和文件元件的編號分開，Presenter 依範圍轉回 session 的編號。

## 證據

- 事實：規則的單元測試不需要視窗：`tst_session`（54 項，含 init／cleanup 與資料列）、`tst_document`、`tst_proposal_session`、`tst_undo_stack`、`tst_state_machine` 全部是 `QTEST_APPLESS_MAIN`（沒有 `QApplication`），每個跑不到 0.3 秒。整合測試 `tst_presenter` 另外驗證接線。
- 事實：變異測試（植入缺陷、看測試是否失敗）：植入 `application/` 與 `domain/` 的缺陷（M01–M15、M25–M27），除了 M12 之外都被對應的單元測試抓到（M12 第一次存活，補上單元測試後抓到；M26 第一次植入的寫法無法編譯，換寫法後抓到）。需要視窗與執行緒才看得出來的缺陷（遲到的結果、焦點、樹的按鍵轉送）只有整合測試 `tst_presenter` 抓得到（結果見 `docs/steps/S04-presenter/perf.md` 的「變異測試」一節）。
- 事實：`ProposalSession` 值語意讓「接受後復原回到預覽，連提案的修改堆疊一起」只是複製一個物件；`tst_document::undoOfAcceptReturnsTheProposalWithItsEditStackAndThreshold` 驗證。
- 假設（未驗證）：S07 接上流程 B 的 matcher、之後的類型編輯器，可以沿用同一個 session 結構（`EditingType` 已在狀態機裡）。

## 否決的替代方案

- **規則寫在 Presenter 裡**（ux-spec 12.4 的直譯）：每條規則的測試都要建立 `QApplication`、視窗、送事件；規則與畫面更新交纏在一起，改一個會動到另一個。
- **用 `QUndoStack`**：application 層不能用 Qt（ADR-002）。
- **可變的 Command ＋ 每次複製整份文件快照**：ADR-002 已否決（成本隨物件數與深度成長）。
- **六個 Presenter 各自持有一部分狀態**：選取、提案、文件三者互相依賴（刪除要看狀態、復原要看狀態），分開持有就要在 Presenter 之間同步，等於重新發明一個 session。

## 後果

- `InspectionSession` 是 application 層裡最大的類別之一（約 270 行）；如果加入類型編輯（流程 C），`TypeEditSession` 應該是 session 的另一個成員，而不是把規則塞進 Presenter。
- `AutoRoiPresenter.cpp` 約 540 行，超過專案「超過 300 行要在 commit 說明」的準則：它是樞紐，每一段都是「規則的結果 → 某個 View」，沒有規則；如果再增加 View，應該先把「發布」拆成獨立的類別。
- S06 的 `IImageSource` 取代 `ImageLoader` 時，`toImageView`（`QImage` → `ImageView`）的轉接可以刪除，Presenter 不必改。
- 如果之後要求「Presenter 一個責任一個類別」（ux-spec 12.4 的原樣），把 `AutoRoiPresenter` 拆成三個類別只需要搬方法，規則不動；那時要新增一筆 ADR 取代本筆的第 4 點。
