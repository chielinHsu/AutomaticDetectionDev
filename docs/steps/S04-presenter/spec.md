# S04 — Presenter、互動狀態機、復原／重做：目的、原因、效果

> 規則：英文區塊用 ASD-STE100 風格（短句、主動語態、一詞一義）。繁中摘要放在每段之後。
> 檢查：`node tools/ste-lint.mjs --strict`（只檢查句長與被動語態，不是完整 STE 驗證）。
> 設計來源：[`docs/plan/S04-presenter.md`](../../plan/S04-presenter.md)、[`ux-spec.md`](../../ux/ux-spec.md) 第 5、6、8、9、12 節、[ADR-002](../../adr/002-undo-model.md)。本檔只講「為什麼這樣切」；實測數字在 [`perf.md`](perf.md)；決策在 [ADR-011](../../adr/011-interaction-logic-in-application-layer.md)、[ADR-012](../../adr/012-dashed-frames-as-fill-rects.md)。

<!--STE-->
## Purpose
This step connects the screen to the core.
It adds the interaction state machine, the undo stacks and the object table.
The user draws a sample and the proposal appears.
<!--/STE-->

**繁中**：把畫面接上核心：互動狀態機、三個範圍的復原堆疊、物件表與參數 dock。使用者框一個樣本，提案就出現；接受、捨棄、復原都可以。

<!--STE-->
## Reason
A search takes time, so the screen must not freeze.
The user must be able to cancel a search.
The user must be able to undo a wrong result.
<!--/STE-->

**繁中**：搜尋要花時間，所以畫面不能凍結；使用者要能取消；自動結果一定有錯，所以修正要便宜（ADR-002）。

<!--STE-->
## Effect
The screen stays responsive while a search runs.
The cancel key returns the screen to idle at once.
An accepted result is one key press from undo.
<!--/STE-->

**繁中**：搜尋時畫面照常反應；取消讓畫面回到 Idle 並重繪（實測中位數 2.5–3.1 ms，p95 3.5–4.9 ms，預算 100 ms；不含 worker 停下的時間）；接受之後一次 Ctrl+Z 就回到預覽（1 054 個框，p95 13.6–15.8 ms，預算 50 ms）。物件表 5 000 列填完（p95 8.0–9.3 ms，預算 50 ms）。搜尋期間事件迴圈最長的一次阻塞 p95 30.9–41.9 ms（預算 50 ms，餘裕只有 16–38%）。**分數門檻拖動時的重繪沒有達到預算**（5 832 個元件 p95 46–61 ms，預算 16 ms）：原因與下一步在 [`perf.md`](perf.md)。數字是同一份程式連跑 3 次的範圍（這台機器會飄 30%）。

<!--STE-->
## Rules live in the application layer
The rules for state, undo and accept are not about the screen.
We write them in classes that have no Qt code.
A unit test checks each rule without a window.
The presenter only connects the rules to Qt.
<!--/STE-->

**繁中**：「按 Enter 之後什麼會發生」「Ctrl+Z 作用在哪個堆疊」「刪除作用在提案還是文件」是規則，不是畫面。規則寫在沒有 Qt 的 `InspectionSession`（`application/interaction/`），所以能用決定性的單元測試逐項驗證（`tst_state_machine`、`tst_session`、`tst_document`、`tst_proposal_session`、`tst_undo_stack`，五個執行檔共 182 個通過項，含各自的 `initTestCase`、`cleanupTestCase` 與資料列）。Presenter（`presentation/presenters/`）只做 Qt 這一側：worker ↔ UI 執行緒、計時器、把資料變成畫面。**與 ux-spec 12.4 的差異**：該表列了六個 Presenter；S04 只做 `AutoRoiPresenter` 與 `ParamsPresenter`（規則在 session，`ComponentTreePresenter` 與 `HistoryPresenter` 的責任各只有幾行，併進 `AutoRoiPresenter`）；`LibraryPresenter`、`TypeEditorPresenter` 不在 S04 的任務清單（見「與計畫的差異」）。

<!--STE-->
## The state machine
The state machine has eight states.
A table lists every legal move.
The state machine refuses a move that the table does not list.
A test checks each row of the table and each refused move.
<!--/STE-->

**繁中**：8 個狀態（`Empty`、`Idle`、`DrawingRegion`、`Searching`、`Preview`、`NoMatch`、`Error`、`EditingType`），轉移表照 ux-spec 第 6 節。`tst_state_machine` 裡有一份獨立抄寫的表：40 列各是一個測試資料列；8 × 19 ＝ 152 組 (狀態, 事件) 中，表裡只有 34 組，其餘 118 組（再乘上條件的 4 種組合）一律必須被拒絕且狀態不變。`DrawingRegion` 記得來源：從預覽開始畫、按 Esc，回到預覽、提案不丟。

<!--STE-->
## Commands and undo
Every change is one command.
A command never changes after we create it.
The undo stack keeps two copies of the data for each change.
So we can share a command between two stacks.
<!--/STE-->

**繁中**：一個使用者意圖一個 `Command`；建立後不可變（`redo`、`undo` 都是 `const`），合併（連續微調）回傳新的 Command，不改舊的。提案的修改是 memento（前後各一份不可變的 `ProposalEdits`）。因此「提案連同它的修改堆疊」可以是便宜的值複本：接受或捨棄時，整份被放進文件堆疊的 Command，復原時原樣還給預覽（`UndoToPreview`）。深度上限 200，超過丟最舊的並釋放它帶的資料；「有未儲存的修改」＝堆疊位置是否等於儲存時的位置（合併進已儲存的那一步、或把它擠出堆疊底部，都讓它再也回不去）。

<!--STE-->
## Which stack does undo use
In the preview state undo uses the proposal stack.
In the idle state undo uses the document stack.
In every other state undo does nothing.
An extra undo in the preview never changes the board.
<!--/STE-->

**繁中**：ADR-002 規則 5。預覽中的 Ctrl+Z 只動提案堆疊；提案堆疊空了，多餘的復原被忽略，不會越過去復原文件。預覽中的 Ctrl+Y：先重做提案的修改；沒有的話，看文件堆疊的下一步是不是「重新接受／捨棄這一份提案」（比較的是同一份不可變提案的指標），是才重新接受。使用者在 `UndoToPreview` 之後改了提案或門檻，舊的「接受」帶的是改之前的提案，就不能再重做（`dropRedo`）。ADR-002 規則 4（接受時先清掉提案堆疊的可重做部分）有專門的測試。

<!--STE-->
## The latest search wins
The presenter starts the search on a worker thread.
The result returns to the screen thread as an unchanged snapshot.
The presenter drops a late result by its generation number.
The screen never waits for the worker.
<!--/STE-->

**繁中**：搜尋交給 `JobRunner`（latest-wins），在 `QtThreadPoolExecutor` 的 worker 上跑。worker 的 lambda 只捕獲自己的資料（請求的複本、結果盒、進度橋），不捕獲 Presenter；結果經 `UiPoster`（一把鎖保護、解構時 `detach`）以 queued 排回 UI 執行緒。`JobReport::latest` 是在 worker 上算的，轉回 UI 後再用 Presenter 自己的「世代」與狀態檢查一次。取消時 UI 立刻回到 Idle，不等 worker（I4 量的是這個）。QThreadPool 實測的坑：`setExpiryTimeout(-1)` 在 Qt 6.8.0 不會保留執行緒，要用 `INT_MAX`（`tst_qt_executor` 抓到）。

<!--STE-->
## Busy, progress and cancel
After 200 ms the status bar shows a busy label.
After one second the message bar shows a progress bar and a cancel button.
A busy label stays at least 400 ms after a result.
A cancel hides the label at once.
<!--/STE-->

**繁中**：ux-spec 第 9 節。忙碌標示（狀態列「搜尋中…」、游標變 `BusyCursor`）> 200 ms 才出現，進度條＋〔取消 (Esc)〕> 1 s 才出現；標示一旦出現，**結果回來時**至少停留 400 ms（避免閃爍；只延後小標示，不延後結果）；使用者**自己取消**時馬上收起來（取消是使用者的決定，「搜尋中…」不能留在 Idle 的畫面上）。進度橋每 40 ms 最多送一次，每個階段結束一定送。搜尋中畫布暫停（`ICanvasInputGate`）：工具不收指標與按鍵，縮放與平移照常；Esc 仍然送得到 Presenter。

<!--STE-->
## The score threshold and the search parameters
The score threshold filters the cached candidates.
It does not start a new search.
The angle range needs a new search.
So the presenter waits 120 ms after the last change.
<!--/STE-->

**繁中**：`ProposalSession::objects()` 對快取的候選重新 `classify`（不碰任何 port），所以拖門檻不搜尋（`tst_presenter` 驗證 `builds`、`finds` 計數不變）。角度範圍等參數經 `ParamsPresenter` 防抖 120 ms，之後 `restartSearch`：進行中的舊搜尋被 latest-wins 取代，三次連續變動只搜尋一次。比對範圍（流程 B 用）只記下來。

<!--STE-->
## The object table
The object table is a tree with a board, components and box groups.
The model keeps one snapshot and builds each cell when the view asks.
A change in state does not reset the model.
The tree keeps its scroll position, its selection and its open nodes.
<!--/STE-->

**繁中**：`ObjectsModel`（`QAbstractItemModel`，用 `QAbstractItemModelTester` 驗證結構）持有 Presenter 交來的一份不可變快照，`data()` 被問到時才算（延遲載入），篩選只是重建「看得到的列」索引。列的編號與順序沒變（門檻拖動）時只發 `dataChanged`，不 reset。**實測抓到的坑**：狀態與分數兩欄原本用 `ResizeToContents`，5 000 列時每次資料變動都要掃描上千列的文字寬度，U2 是 98 ms；改成由字型量出來的固定寬度後中位數 5.8–6.3 ms。選取是雙向同步的：使用者點樹 → `ISelectionController`；Presenter 設定樹的選取不會再回報。畫布與樹用同一個整數編號；提案物件加上 `kProposalIdBase`（1 000 000），和文件裡的元件編號分開，所以預覽時背景的文件元件不會被誤選。

<!--STE-->
## Region scan has no library yet
The region scan needs a sample library.
The library store and the matcher do not exist yet.
So a region scan returns the reason that the library is empty.
The card offers to teach a new sample.
<!--/STE-->

**繁中**：流程 B（區域掃描）的 `ILibraryStore`、`ILibraryMatcher` 在 S06、S07 才有實作。庫裡確實沒有任何樣本，所以結果是 `NoMatch`（`LibraryEmpty`），卡片建議〔教一個新樣本〕，按下去切到「教樣本」工具。這是真的狀況，**不是假裝比對過**：測試驗證沒有呼叫教樣本的 back end（`builds ＝ 0`）。狀態機和卡片已經涵蓋流程 B 的狀態，S07 接上 matcher 時只換 `finishRegionScan`。

<!--STE-->
## What this step does not do
This step does not build the type editor or the library panel.
It does not save a board or a library.
It does not switch the back end while the program runs.
<!--/STE-->

**繁中**：不做類型編輯器（`EditingType` 在狀態機裡，雙擊元件只顯示「類型編輯器尚未提供」）、樣本庫面板與同步動作、〔接受並存入樣本庫〕、辨識候選（`chooseAlternative`）、存檔、執行中切換 back end（目前只有一個可用的；上方選單顯示目前用的，停用）。元件名稱的開頭字元：暫時類型沒有類別，所以叫 `X1`、`X2`…（有類別後 `R`、`U`）；µm 與像素的換算在影像未校正時暫定 1 µm ＝ 1 像素，與校正值一起在 S06 定案。

## 元件與檔案

| 層 | 內容 |
|---|---|
| `domain` | `box_expander.h`（`BoxGroup` ＋ 位姿 → 框：Body、Pin 矩陣、Window 比例；`effectiveUmPerPixel`） |
| `application/document` | `UndoStack.h`（樣板、`Command`）、`ProposalSession`（門檻＋提案修改堆疊，值語意）、`InspectionDocument`（板子、五種 Command、一次通知） |
| `application/interaction` | `InteractionStateMachine`（無條件轉移表）、`InspectionSession`（狀態機＋文件＋提案＋選取，所有規則） |
| `infrastructure/threading` | `QtThreadPoolExecutor`（`IExecutor` 的 `QThreadPool` 版；`aoi_threading_qt`） |
| `presentation/presenters` | `AutoRoiPresenter`、`ParamsPresenter`、`AppPresenters`（接線）、`SceneBuilder`（`buildScene`）、`MessageTexts`（卡片）、`UiPoster`、`ViewInterfaces.h`、`ControllerInterfaces.h` |
| `presentation/objects`、`params` | `ObjectsModel`、`ObjectsPanel`、`ObjectRows.h`；`ParamsPanel` |
| `presentation` 既有 | `MainWindow`（變成 View：不再有 `ShellController`）、`ImageCanvas`（輸入閘門、鍵盤、`activeToolChanged`）、`MessageBar`（進度列、按鈕沿用）、`SelectTool`（拖曳移動）、`BoxFrames`／`OverlayStyle`／`RoiLayerItem`（虛線與點線改用填色矩形，ADR-012） |
| `app` | `composeCore` 多一個 `ExecutorKind`；`main.cpp` 組裝 `AppPresenters` |
| 測試 | `tests/core/`：`tst_undo_stack`、`tst_state_machine`、`tst_box_expander`、`tst_proposal_session`、`tst_document`、`tst_session`、`tst_qt_executor`；`tests/ui/`：`tst_presenter`（整合）、`tst_objects_model`、`tst_params`、`tst_messages`、`tst_scene`，以及 `tst_tools`、`tst_items`、`tst_shell` 的增補、`perf_ui_s04` |

## SOLID 對照

| 原則 | 這一步的具體做法 | 驗證 |
|---|---|---|
| **S** | 狀態機只管轉移表；`InspectionSession` 只管規則；`ProposalSession` 只管一份提案與它的修改；`InspectionDocument` 只管已接受的板子；`AutoRoiPresenter` 只管 Qt 這一側；`SceneBuilder` 只管資料轉換；`ObjectsModel` 只管樹的資料。兩個檔案**超過 300 行**：`AutoRoiPresenter.cpp`（707 行：它是樞紐，每一段都是「規則的結果 → 某個 View」，沒有規則）與 `InspectionDocument.cpp`（387 行：五個 Command 各有一段「做」與「復原」，結構相同）。要不要拆開是下一個動它們的人的判斷，這一步沒有拆 | 程式審查（行數）；每個類別一個測試檔 |
| **O** | 新的 Command ＝ 新增一個類別，不改 `UndoStack`；新的 View 實作（例如之後的 QML）＝ 實作同樣的窄介面；新的 back end ＝ 一組 adapter ＋ 註冊（S03 的結論沒被破壞） | `tst_undo_stack` 用 `Counter` 目標測 `UndoStack`（完全不認識文件） |
| **L** | `QtThreadPoolExecutor` 與 `ThreadPoolExecutor` 都滿足 `IExecutor` 的契約（FIFO、解構時做完已排入的工作、吞掉例外、不阻塞 post）；同一個 `tst_presenter` 流程在兩者上都通過 | `tst_qt_executor`、`theStdThreadExecutorWorksTheSameWay` |
| **I** | View 介面各 ≤ 5 個方法（`IStatusView` 5、`IMessageView` 2、`IHistoryView` 2、`IParamsView` 3、`IComponentTreeView` 3、`ICanvasInputGate` 2）；Controller 介面各 ≤ 5；`ICanvasController` 沒有為了「按下」多加第 6 個方法（改用畫布既有的 `pointerDragChanged` 訊號） | 程式審查（方法數） |
| **D** | Presenter 只依賴 application 的型別（use case 的參考、`JobRunner`）與 View／Controller 介面；沒有 `new` 任何 adapter；`check-deps` 讓 presentation 不得 include infrastructure | `check_deps`（CTest）；`presentation/CMakeLists.txt` 只連 `aoi_application` |

## 決策審查（engineering-workflow 第 3 節）

| 面向 | Decision | Evidence | Verification |
|---|---|---|---|
| 架構與模組介面 | 規則放 application 層（[ADR-011](../../adr/011-interaction-logic-in-application-layer.md)）；View／Controller 窄介面；`AppPresenters` 是唯一的接線，測試走的就是產品的接線 | `diagram.md` §1；`check_deps` 通過 | `tst_presenter` 用 `AppPresenters` 組裝 |
| 資料契約與生命週期 | `Command` 與 `ProposalEdits` 不可變；`SampleProposal` 以 `shared_ptr<const>` 在執行緒之間傳；`ImageView` 帶 `owner`，工作結束前像素一定活著（`toImageView` 用 `shared_ptr<QImage>`） | `tst_scene`：原圖解構後 view 仍可讀 | 同左；`tst_proposal_session::aCopyCarriesTheWholeProposal…` |
| 所有權與資源釋放 | 解構順序：`AppPresenters`（先解除視窗的 Controller）→ `MainWindow` → 核心。`~AutoRoiPresenter` 先 `UiPoster::detach()` 再 `runner.cancel()`；已排進事件佇列的 queued 呼叫隨 `QObject` 解構被 Qt 丟掉 | `AppPresenters.h`、`UiPoster.h` 的註解 | `tst_presenter` 的 `~Rig`（每個測試都在搜尋中或之後解構） |
| 並行、順序與狀態 | worker 只碰「自己的」資料；UI 狀態只在 UI 執行緒改；`UiPoster` 的鎖保證「worker 正在排事件」與「Presenter 正在解構」不會同時發生；`done` 在 `invoke` 之前就可能已經完成（InlineExecutor），所以世代在 `submit` **之前**指定 | `diagram.md` §4 | `aNewSearchReplacesAnOlderOne…`、`openingAnotherImageWhileSearching…`、`aSlowSearchShows…` |
| 可靠性與錯誤處理 | 無結果是 `NoMatch`（有原因與下一步）；失敗是 `Error`（可再試）；影像打不開是 `Error`（Dismiss 回 Idle 或 Empty）；搜尋中收到打不開的影像，不打斷搜尋，只用吐司說一聲 | `MessageTexts` 對每個 `NoMatchReason` 都有卡片與〔關閉〕 | `tst_messages`（12 個原因、兩種語言文案齊全） |
| 安全與信任邊界 | 本步驟沒有新的外部輸入（影像檔的解碼沿用 S02 的 Qt 路徑，S06 取代） | — | — |
| 效能與容量 | 先量再改。量到 U2 的 98 ms 瓶頸在 `ResizeToContents`（改了）；I3 的 111 ms（p95）與 U3 的重繪瓶頸在 S02 的點線畫法（改成填色矩形，ADR-012，微基準 425 → 27.5 ms）。**U3 仍超標**，原因與選項在 perf.md | `perf.md` | `perf_ui_s04`（label perf；Release 才把預算當失敗條件，U3 列為已知超標） |
| 交付、相容性與可觀測性 | `composeCore` 的簽名向後相容（新參數有預設值）；畫面文案新增 12 個 key、移除 5 個暫代 key（`ux-spec.md` 修改紀錄）；Presenter 每個發布步驟有 `PerfProbe` 計時（預設關閉） | `docs/ux/copy.json` | `tst_messages::theNewCopyKeysExist…` |
| 測試與驗證 | Qt Test；整合測試走真的 `QThreadPool` 與滑鼠／鍵盤事件；變異測試（結果在 perf.md 同層的交接單與 changelog） | `tests/` | `scripts/dev/test.ps1`（Release 與 Debug） |

## 與計畫的差異（誠實紀錄）

- **規則放在 application 層**（ADR-011）：計畫寫「狀態機不含 Qt、`InspectionDocument` 在 application 層」；多做的是把「接受／復原／刪除／門檻」整套規則做成 `InspectionSession`，Presenter 變薄。
- **只有兩個 Presenter**：ux-spec 12.4 列了六個。`ComponentTreePresenter` 與 `HistoryPresenter` 併進 `AutoRoiPresenter`；`LibraryPresenter`、`TypeEditorPresenter` 不在 S04 的任務清單，**這兩個沒有任何一步的 plan 檔涵蓋流程 C（類型編輯器）與樣本庫面板的 UI**，需要使用者決定放進哪一步。
- **改了 S02 的繪製程式**（ADR-012）：虛線與點線原本走 cosmetic 描邊，S02 假設「這兩種狀態的框很少」；S04 的預覽讓使用者把門檻拉高時，一次有上千個框變成點線。量測後改成填色矩形。這不在計畫裡，但 U3、I3 是 S04 的預算。
- **新增 `ICanvasInputGate`、`IKeyboardController`、`outlineOf`**：畫布在搜尋中要暫停、按鍵要交給 Presenter、拖曳要畫外框，都是 S02 的介面沒有的。
- **`SelectTool` 加了拖曳移動**與方向鍵微調：`onComponentsMoved` 從 S02 就有，但沒有工具會呼叫它；ADR-002 規則 7（連續拖曳是一個 Command）需要它。
- **U3 超標**：見 perf.md。預算沒改。
- **`ShellController`、5 個 `ui.shell.*` 暫代文案移除**；`MainWindow` 單獨存在時沒有狀態列摘要與提示（Presenter 發布）。
- **還沒有在 Qt Creator 裡載入 `AiInspTool.pro`**：只驗證了命令列 qmake ＋ jom 建置。
