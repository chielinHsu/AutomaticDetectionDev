# S04 — 圖解

> 圖都從程式碼推導（節點標檔名或函式名）。語法用 mermaid 11 的解析器檢查過（只驗語法，沒有渲染）。
> 設計說明在 [`spec.md`](spec.md)，實測數字在 [`perf.md`](perf.md)，決策在 [ADR-011](../../adr/011-interaction-logic-in-application-layer.md)、[ADR-012](../../adr/012-dashed-frames-as-fill-rects.md)。

## 1. 誰認識誰（分層與接線）

規則（狀態、復原、接受時發生什麼事）在沒有 Qt 的 application 層；Presenter 只做 Qt 這一側（執行緒、計時、把資料畫出來）；View 只有窄介面。`app/main.cpp` 是唯一同時認識所有人的地方。

```mermaid
flowchart LR
  subgraph app["app（組裝根）"]
    main["main.cpp<br/>composeCore + AppPresenters"]
  end
  subgraph pres["presentation"]
    mw["MainWindow<br/>IStatusView, IMessageView, IHistoryView"]
    canvas["ImageCanvas<br/>ICanvasView, ICanvasInputGate"]
    objp["ObjectsPanel<br/>IComponentTreeView"]
    parp["ParamsPanel<br/>IParamsView"]
    autop["AutoRoiPresenter<br/>7 個 Controller 介面"]
    parpres["ParamsPresenter<br/>IParamsController"]
    scene["SceneBuilder<br/>buildScene()"]
    poster["UiPoster<br/>worker 到 UI"]
  end
  subgraph appl["application（無 Qt）"]
    sess["InspectionSession"]
    fsm["InteractionStateMachine"]
    doc["InspectionDocument<br/>UndoStack of Command"]
    prop["ProposalSession<br/>值語意、不可變的 memento"]
    uc["GenerateObjectsFromSample"]
    runner["JobRunner (latest-wins)"]
  end
  subgraph infra["infrastructure"]
    qtexec["QtThreadPoolExecutor"]
    fake["backend_fake"]
  end
  main --> autop
  main --> parpres
  main --> qtexec
  main --> fake
  mw --> autop
  canvas --> autop
  objp --> autop
  parp --> parpres
  parpres --> autop
  autop --> sess
  autop --> scene
  autop --> runner
  autop --> poster
  sess --> fsm
  sess --> doc
  sess --> prop
  runner --> uc
  runner --> qtexec
  uc --> fake
  scene --> sess
  autop -. "showComponents / setRows / showCard" .-> mw
  autop -. "showComponents / setSelection" .-> canvas
  autop -. "setRows / setSelection" .-> objp
```

## 2. 互動狀態機（8 個狀態）

轉移表照 `docs/ux/ux-spec.md` 第 6 節；`tst_state_machine` 把表裡的 40 列各驗一次，其餘 118 組 (狀態, 事件) 驗證一律被拒絕。`EditingType` 在狀態機裡，但編輯器本身不在 S04。

```mermaid
stateDiagram-v2
  [*] --> Empty
  Empty --> Idle: ImageOpened
  Empty --> Error: ImageFailed
  Idle --> DrawingRegion: PressRegion
  Idle --> EditingType: EditType
  Idle --> Preview: UndoToPreview
  Idle --> Idle: ImageOpened
  Idle --> Error: ImageFailed
  DrawingRegion --> Searching: Release
  DrawingRegion --> Idle: Esc or ReleaseTooSmall (from Idle)
  DrawingRegion --> Preview: Esc or ReleaseTooSmall (from Preview)
  DrawingRegion --> NoMatch: Esc or ReleaseTooSmall (from NoMatch)
  Searching --> Preview: ResultFound
  Searching --> NoMatch: ResultEmpty
  Searching --> Error: Failure
  Searching --> Idle: Cancel or ImageOpened
  Searching --> Searching: ParamsChanged
  Preview --> Preview: ThresholdChanged
  Preview --> Searching: ParamsChanged
  Preview --> Idle: Accept or Discard or ImageOpened
  Preview --> DrawingRegion: PressRegion
  Preview --> Error: ImageFailed
  NoMatch --> Preview: ThresholdChanged (found one)
  NoMatch --> NoMatch: ThresholdChanged (found none)
  NoMatch --> Searching: ParamsChanged
  NoMatch --> Idle: Dismiss or ImageOpened
  NoMatch --> DrawingRegion: PressRegion
  NoMatch --> Error: ImageFailed
  Error --> Idle: Dismiss (has image) or ImageOpened
  Error --> Empty: Dismiss (no image)
  Error --> Searching: Retry
  EditingType --> Idle: Save or Cancel
```

## 3. Command 與復原（類別圖）

Command 建立後不可變，所以同一個 Command 可以被多份堆疊共用；合併回傳新的 Command，不改舊的。`ProposalSession` 是值型別：接受或捨棄時，整份（含它的修改堆疊）被複製進文件堆疊的 Command。

```mermaid
classDiagram
  class Command~Target~ {
    <<abstract>>
    +redo(Target) const
    +undo(Target) const
    +kind() CommandKind
    +count() size_t
    +mergeWith(next) shared_ptr
  }
  class UndoStack~Target~ {
    +push(target, cmd, allowMerge)
    +undo(target) bool
    +redo(target) bool
    +dropRedo()
    +markClean()
    +isClean() bool
    -maxDepth 200
  }
  class InspectionDocument {
    +accept(session) size_t
    +discard(session)
    +removeComponents(ids)
    +addManualComponent(pose, type)
    +moveComponents(ids, delta, merge)
    +undo() UndoOutcome
    +redo() UndoOutcome
    +nextRedoProposal() ProposalSession
    -Board board_
    -Counters counters_
  }
  class DocumentCommand {
    <<abstract>>
    +proposal() ProposalSession
  }
  class AcceptCommand
  class DiscardCommand
  class DeleteCommand
  class AddCommand
  class MoveCommand
  class ProposalSession {
    +objects() ProposalObjects
    +setThreshold(t)
    +deleteObjects(ids)
    +addManual(pose)
    +moveObjects(ids, delta, merge)
    +undo() bool
    +redo() bool
  }
  class EditsCommand
  class ProposalState {
    shared_ptr ProposalEdits edits
  }
  class InspectionSession {
    +accept()
    +discard()
    +undo()
    +redo()
    +setThreshold(t)
  }
  Command <|-- DocumentCommand
  DocumentCommand <|-- AcceptCommand
  DocumentCommand <|-- DiscardCommand
  DocumentCommand <|-- DeleteCommand
  DocumentCommand <|-- AddCommand
  DocumentCommand <|-- MoveCommand
  Command <|-- EditsCommand
  UndoStack o-- Command
  InspectionDocument *-- UndoStack : Target = InspectionDocument
  ProposalSession *-- UndoStack : Target = ProposalState
  ProposalSession *-- ProposalState
  EditsCommand ..> ProposalState : before / after 兩份 memento
  AcceptCommand *-- ProposalSession : 帶一份複本
  DiscardCommand *-- ProposalSession : 帶一份複本
  InspectionSession *-- InspectionDocument
  InspectionSession *-- ProposalSession : 預覽時才有
```

## 4. 搜尋：UI 執行緒與 worker（序列圖）

UI 執行緒只做三件事：發出工作、收結果、畫畫面。結果以不可變的 `SampleProposal` 經 `UiPoster` 排回 UI；過期的結果靠世代丟掉。

```mermaid
sequenceDiagram
  autonumber
  participant U as 使用者
  participant C as ImageCanvas (UI)
  participant P as AutoRoiPresenter (UI)
  participant S as InspectionSession (UI)
  participant R as JobRunner
  participant W as worker (QThreadPool)
  participant Q as UiPoster

  U->>C: 放開滑鼠
  C->>P: onRegionDrawn(rect, Sample)
  P->>S: endRegion(rect) 進入 Searching
  P->>P: ++generation_，beginBusy() 啟動 200 ms 與 1 s 計時
  P->>R: submit(work, done)
  P-->>C: publishAll() 保留拖出的框、畫布暫停
  Note over P: 到這裡 UI 執行緒回到事件迴圈，沒有被擋住
  R->>W: post(work)
  W->>W: use case.run(request, cancel, progress)
  W-->>Q: report(進度) 每 40 ms 最多一次
  Q-->>P: queued onProgress(gen, fraction)
  alt 超過 200 ms
    P-->>C: showBusy(Busy) 狀態列與游標
  end
  alt 超過 1 s
    P-->>C: showBusy(Progress) 訊息列的進度條與取消
  end
  alt 使用者按 Esc（取消）
    U->>P: cancelSearch()
    P->>P: ++generation_，runner.cancel()
    P->>S: cancelSearch() 回到 Idle
    P-->>C: publishAll()，吐司「已取消搜尋」
    Note over P,S: UI 在 100 ms 內回到 Idle（I4），不等 worker
  end
  W-->>R: work 結束
  R-->>Q: done(JobReport) 在 worker 上呼叫
  Q-->>P: queued onJobFinished(gen, report, outcome)
  alt generation 不符 或 狀態不是 Searching
    P->>P: 丟掉這個結果
  else 現行的結果
    P->>S: searchSucceeded(proposal) → Preview 或 NoMatch
    P-->>C: publishAll() 框、物件樹、訊息卡、狀態列
  end
```

## 5. 接受、復原、重做（提案的生命週期）

「接受」是文件堆疊上的一個 Command，帶著當時的提案。復原它，狀態回到 `Preview`（`UndoToPreview`），提案連同它的修改堆疊與門檻一起回來。

```mermaid
sequenceDiagram
  autonumber
  participant U as 使用者
  participant S as InspectionSession
  participant PS as ProposalSession
  participant D as InspectionDocument

  Note over S,PS: 狀態 Preview，提案有 3 步修改
  U->>S: Enter（accept）
  S->>PS: dropRedo() 清掉提案堆疊殘留的重做（ADR-002 規則 4）
  S->>D: accept(PS) 建立 AcceptCommand，帶 PS 的複本
  D-->>S: 新增 N 個元件與一個暫時類型；通知一次
  S->>S: fire(Accept) → Idle，清掉提案與選取
  U->>S: Ctrl+Z（undo）
  S->>D: undo()
  D-->>S: UndoOutcome 帶回 PS 的複本
  S->>S: 還原提案、門檻、區域；fire(UndoToPreview) → Preview
  U->>S: Ctrl+Y（redo）
  S->>PS: 提案堆疊沒有可重做的
  S->>D: nextRedoProposal() 與目前的提案是同一份嗎
  alt 是同一份
    S->>D: redo() 再接受一次 → Idle
  else 不是（改了提案、換了門檻、或重新搜尋過）
    S->>S: 什麼都不做
  end
```

## 6. Ctrl+Z 作用在哪個堆疊

```mermaid
flowchart TD
  K["Ctrl+Z / Ctrl+Y"] --> Q{"狀態"}
  Q -- "Preview" --> P["提案堆疊<br/>不會越過去動文件歷史"]
  Q -- "Idle" --> D["文件堆疊"]
  Q -- "Searching / DrawingRegion / NoMatch / Error / Empty" --> N["忽略"]
  P --> P2{"提案堆疊有可做的嗎"}
  P2 -- "有" --> P3["undo / redo 提案修改"]
  P2 -- "沒有，且是 redo" --> P4{"文件堆疊的下一步是<br/>重新接受這一份提案嗎"}
  P4 -- "是" --> P5["redo 文件：Accept 或 Discard → Idle"]
  P4 -- "否" --> N
  P2 -- "沒有，且是 undo" --> N
  D --> D2{"被復原的是接受或捨棄嗎"}
  D2 -- "是" --> D3["UndoToPreview：提案回來"]
  D2 -- "否" --> D4["只改文件"]
```

## 7. Esc：往回一步

```mermaid
flowchart TD
  E["Esc"] --> S{"狀態"}
  S -- "DrawingRegion" --> T["工具放棄這個框（不經過 Presenter）"]
  S -- "Searching" --> C["cancelSearch：回到 Idle"]
  S -- "Preview" --> Sel{"有選取嗎"}
  Sel -- "有" --> ClearSel["清除選取"]
  Sel -- "沒有" --> Disc["discard：捨棄提案（可復原）"]
  S -- "NoMatch / Error" --> Dis["dismiss：回到 Idle 或 Empty"]
  S -- "Idle" --> Sel2{"有選取嗎"}
  Sel2 -- "有" --> ClearSel
  Sel2 -- "沒有" --> Nothing["不處理"]
```

## 8. 發布：規則的結果怎麼變成畫面

每次狀態或內容變了，Presenter 用 `publishAll()` 一次把畫面對齊工作階段。分數門檻拖動時也是這一條（沒有另一條「快速路徑」，量測後再決定要不要，見 perf.md）。

```mermaid
flowchart LR
  A["InspectionSession<br/>狀態、文件、提案、選取"] --> B["buildScene()<br/>classify + 修改 + BoxExpander"]
  B --> C1["ImageCanvas.showComponents<br/>snapshot"]
  B --> C2["ObjectsPanel.setRows<br/>形狀沒變只發 dataChanged"]
  B --> C3["MainWindow.showSummary"]
  A --> D1["canvas.setSelection / tree.setSelection"]
  A --> D2["canvas.showRegionDraft<br/>掃過的區域"]
  A --> D3["previewCard / noMatchCard / errorCard<br/>MainWindow.showCard"]
  A --> D4["undoInfo / redoInfo<br/>MainWindow.setUndo / setRedo"]
  A --> D5["setToolsEnabled / setBusyCursor"]
```

## 9. 修改前後

```mermaid
flowchart TD
  subgraph before["S02 / S03：畫面與核心各自存在"]
    b1["ImageCanvas 的操作"] --> b2["ShellController<br/>只顯示『尚未接上』"]
    b3["composeCore → CoreServices"] --> b4["main.cpp 只驗證 --backend<br/>沒有人使用"]
  end
  subgraph after["S04：接上"]
    a1["ImageCanvas 的操作<br/>滑鼠、鍵盤"] --> a2["AutoRoiPresenter"]
    a2 --> a3["InspectionSession<br/>規則 + 單元測試"]
    a2 --> a4["JobRunner → use case<br/>QThreadPool worker"]
    a4 -- "UiPoster，queued" --> a2
    a2 --> a5["View 介面<br/>畫布、物件樹、參數、訊息列、狀態列、復原鈕"]
  end
  before --> after
```
