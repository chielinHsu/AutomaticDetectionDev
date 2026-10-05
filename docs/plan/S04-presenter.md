# S04 — Presenter、互動狀態機、復原/重做

| 項目 | 內容 |
|---|---|
| 層級 | 系統架構（介面 ↔ 核心） |
| 分支 | `step/04-presenter` |
| 依賴 | S02、S03 |
| 產出位置 | `src/presentation/presenters/`、`src/application/document/`、`tests/ui/`、`docs/steps/S04-presenter/` |

<!--STE-->
## Purpose
This step connects the screen to the core. It adds the interaction state machine, the object table and undo.

## Reason
The screen must stay smooth while a search runs. The user must be able to cancel and to undo.

## Effect
The user draws a sample and the proposals appear. The screen does not freeze. A wrong result is one key press from undo.
<!--/STE-->

**繁中**：把畫面接上核心。搜尋時畫面不凍結、可取消、可復原。此時仍使用假的 searcher。

## 任務

1. **`InspectionDocument`**（application 層、純 C++）：持有物件集合；所有修改走 **Command**（新增/刪除/移動/改狀態/接受提案）；`UndoStack` 支援合併連續移動；ADR-002 的模型落地。
2. **互動狀態機**（手寫 enum FSM，便於測試）：Idle／DrawingSample／Searching／Preview／Error，轉移表照 `S00 diagram.md` §4 與 S01 修訂。**狀態機不含 Qt**，每個轉移有單元測試。
3. **`AutoRoiPresenter`**：把畫布事件→use case；透過 `IExecutor`（Qt 的 `QThreadPool` adapter）在 worker 執行；結果用 queued signal 回 UI 執行緒；**只傳不可變快照**。
4. **進度與取消 UI**：> 200 ms 顯示忙碌、> 1 s 顯示進度＋取消；Esc 取消。
5. **Objects dock**：`QAbstractItemModel`（延遲載入、可依狀態篩選）；與畫布**雙向選取同步**；5 000 列不卡。
6. **Params dock**：簡易／進階兩種模式；滑桿變動以 **debounce＋latest-wins** 重新計算（拖曳中不要排滿工作佇列）。
7. **提案流程**：Preview 狀態的框以「提案」樣式顯示；Enter 接受（一個 Command）、Esc 捨棄。
8. **空白／錯誤／無匹配畫面**：用 S01 的文案，給出下一步建議。

## SOLID 重點

- **S**：狀態機、Presenter、Model 各自單一責任。
- **I**：View 只看到 `IAutoRoiView` 這類窄介面（不是整個 MainWindow）。
- **D**：Presenter 依賴 use case 與 `IExecutor` 介面，不 new 具體類別（由組裝根注入）。

## 效能檢查（寫入 `perf.md`）

U2（表格 5 000 列）、U3（滑桿→重繪）、**I3（搜尋中事件迴圈最長阻塞）**、I4（取消→Idle）、I6（復原 1 000 框的 commit）。

## 四種輸出（本步驟具體內容）

| # | 輸出 | 內容 |
|---|---|---|
| 1 | spec.md | 為什麼用「狀態機＋Command＋快照」的 Purpose/Reason/Effect；SOLID 表 |
| 2 | diagram.md | 狀態機圖、Command/Undo 類別圖、UI 執行緒 ↔ worker 序列圖 |
| 3 | explainer.html | 互動「狀態機模擬器」：按事件按鈕看狀態轉移與畫面變化；可模擬「搜尋很慢」看進度/取消出現的時機 |
| 4 | storyboard.md | 60 秒「搜尋時畫面為什麼不會卡」 |

## 完成定義

- [ ] 狀態機轉移測試全過；Command/Undo 測試過
- [ ] 用假 back end 可完整走一遍：框 → 提案 → 接受 → 復原
- [ ] U2、U3、I3、I4、I6 已量測並與預算比對
- [ ] `node tools/ste-lint.mjs --strict` 無 error

## Commit 範本

```
feat(ui): add presenters, interaction state machine and undo/redo

- InspectionDocument with Command-based edits and UndoStack
- Qt-free interaction state machine with transition tests
- AutoRoiPresenter running jobs on a worker pool, latest-wins with debounce
- Objects/Params docks with selection sync; progress and cancel UI
- U2, U3, I3, I4, I6 measured in perf.md
```

## Start prompt（開新 session 時貼上）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續 AOI 專案。請先讀 CLAUDE.md、docs/STATUS.md、docs/plan/S04-presenter.md，
再依 CLAUDE.md 的 Step Protocol 完成步驟 S04（分支 step/04-presenter）。只做 S04。
完成後：commit、合併到 main、更新 STATUS.md、對我提示 commit 內容，並用 spawn_task 為 S05 開新 session。
```
