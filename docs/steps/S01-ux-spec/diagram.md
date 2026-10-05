# S01 — 圖解

圖在 GitHub、VS Code（Markdown Preview Mermaid）與 Qt Creator 的 Markdown 預覽中可渲染。互動版（可以真的走一遍）見 [explainer.html](explainer.html)。線框的 ASCII 草圖在 [`docs/ux/ux-spec.md`](../../ux/ux-spec.md) 第 4、5 節。

## 1. 三個流程

流程 A（教樣本）、B（區域掃描）、C（編輯元件類型）怎麼接在一起；A 的結果可以存成樣本庫條目，B 找不到的元件回到 A。

```mermaid
flowchart TD
  O(["Open image"]) --> I["Loaded"]
  I -->|"S: draw a box around ONE part"| TS["Searching: find the same parts"]
  I -->|"R: drag ANY region"| SS["Searching: match with every library sample"]
  TS --> PV["Preview: components and their boxes"]
  SS --> PV
  TS -->|"nothing found"| NM["NoMatch: advice in the message bar"]
  SS -->|"nothing found"| NM
  NM -->|"library empty or no match: Teach a new sample"| I
  PV -->|"parts not in the library"| TN["Teach a new sample"]
  TN --> I
  PV -->|"pick another sample for one component"| PV
  PV -->|"score slider: instant filter"| PV
  PV -->|"Enter"| AC["Accepted: components R1, R2, U1 ..."]
  PV -->|"Accept and save to library"| ED
  AC -->|"double-click a component, or E"| ED["Edit component type"]
  I -->|"Library: Edit"| ED
  ED -->|"Save: all components of the type update"| AC
  AC -->|"Export"| CAD["CAD list: name, X, Y, angle, type"]
  PV -->|"Esc twice"| DS["Discarded"]
  DS -->|"Ctrl+Z"| PV
  AC -->|"Ctrl+Z"| PV
```

這張圖要讓你看到：每個死路（無匹配）都有下一步；教出來的樣本進入樣本庫，下一次就能用流程 B；接受與捨棄都能用 `Ctrl+Z` 回到預覽。

## 2. 互動狀態機（修訂 S00 §4）

8 個狀態。與 S00 的差異：新增 `Empty`、`NoMatch`、`EditingType`，移除 `Committed`，`DrawingSample` 改名 `DrawingRegion`，新增 `UndoToPreview`。`DrawingRegion` 結束時回到它開始前的狀態（圖中三條回來的線）。

```mermaid
stateDiagram-v2
  [*] --> Empty
  Empty --> Idle: ImageOpened
  Empty --> Error: ImageFailed
  Idle --> DrawingRegion: PressRegion
  Idle --> Preview: UndoToPreview
  Idle --> EditingType: EditType
  EditingType --> Idle: Save or Cancel
  DrawingRegion --> Searching: Release
  DrawingRegion --> Idle: Esc or too small, came from Idle
  DrawingRegion --> Preview: Esc or too small, came from Preview
  DrawingRegion --> NoMatch: Esc or too small, came from NoMatch
  Searching --> Preview: ResultFound
  Searching --> NoMatch: ResultEmpty
  Searching --> Error: Failure
  Searching --> Idle: Cancel
  Searching --> Searching: ParamsChanged, latest wins
  Preview --> Preview: ThresholdChanged
  Preview --> Searching: ParamsChanged
  Preview --> Idle: Accept or Discard
  Preview --> DrawingRegion: PressRegion
  NoMatch --> Preview: ThresholdChanged, at least one Found
  NoMatch --> Searching: ParamsChanged
  NoMatch --> Idle: Dismiss
  NoMatch --> DrawingRegion: PressRegion
  Error --> Idle: Dismiss
  Error --> Empty: Dismiss, no image loaded
  Error --> Searching: Retry
```

這張圖要讓你看到：只有 `Searching` 會自己離開（結果、失敗、取消）；`EditingType` 只能從 `Idle` 進出，所以編輯類型時不會有搜尋在背景改畫面。`ImageOpened` 在大多數狀態都會回到 `Idle`（為了圖面乾淨未畫出，完整表在 `ux-spec.md` 第 6 節）。

## 3. 資料模型：Board › Component › Box（ADR-006）

框不單獨儲存，而是由「元件類型」與「元件位姿」推導出來。改類型，所有同類型元件一起更新。

```mermaid
classDiagram
  class Board {
    image
    name
  }
  class ComponentInstance {
    name R12 or U1
    x y angle side
    state and score
    origin Auto or Manual
  }
  class ComponentType {
    partNo 8371
    aliases
    name
    category Chip IC Other
    referenceImage
    libId on a board copy
  }
  class BoxGroup {
    <<abstract>>
  }
  class Body {
    one per type
    x y w h
  }
  class Pin {
    x y w h
    rows cols
    pitchX pitchY
    skipped cells
  }
  class Window {
    algo Void Solder Color
    parent
    offset and size as ratios of the parent
  }
  Board "1" *-- "many" ComponentInstance
  ComponentInstance --> ComponentType : type reference
  ComponentType "1" *-- "many" BoxGroup
  BoxGroup <|-- Body
  BoxGroup <|-- Pin
  BoxGroup <|-- Window
  Window --> BoxGroup : parent
```

這張圖要讓你看到：8371 ＝ 1 個 Body ＋ 1 個 Pin 矩陣（2 列 × 10 欄）＋ 1 組 Solder 窗（每支腳一個）＝ 41 個框，但只存 3 個群組。矩陣的「一次建立多個框」與窗的「相依」都是這個結構的自然結果。

## 4. 元件類型編輯器：改一個參數，整組框一起變

```mermaid
flowchart LR
  P["Pin group: w 9 to 12, cols 10 to 12"] --> PX["24 pin boxes"]
  P -->|"linked parent"| W["Solder window group: width 140 percent of the pin"]
  W --> WX["24 window boxes follow: 12.6 to 16.8 wide"]
  PX --> SV["Save"]
  WX --> SV
  SV --> UP["Every component of this type on the board updates in one step"]
  SV --> UD["Ctrl+Z puts all of them back"]
```

數字來自原型的測試：Pin 寬 9 → 12，窗寬 12.6 → 16.8；列數改 12 得 24 支 Pin 與 24 個窗；儲存後板上 3 顆 IC 從 41 個框變 33 個框（欄數改 8），`Ctrl+Z` 後回到 41。

## 4b. 樣本庫與板子：複本關係（ADR-006 §5，使用者決定）

```mermaid
flowchart LR
  LIB[("Library: master of 8371")]
  BC["Board copy p_8371, libId 8371"]
  LIB -->|"Accept: copy the types in use"| BC
  BC -->|"Edit and Save: every component of the type updates"| BC
  BC -->|"Add to the library: overwrites the entry"| LIB
  LIB -->|"Update from the library"| BC
  LIB -->|"Edit the library entry: board copies stay as they are"| LIB
  LIB -.->|"Delete the entry: board copies stay"| BC
```

這張圖要讓你看到：庫與板子只透過兩個明確的動作同步（〔從樣本庫更新〕、〔存入樣本庫〕），都是文件堆疊上的 Command，可復原；改庫、刪庫都不會偷偷改到舊板子。

## 5. 區域掃描怎麼辨識（流程 B）

```mermaid
sequenceDiagram
  actor U as User
  participant P as AutoRoiPresenter
  participant J as Worker (JobRunner)
  participant M as ILibraryMatcher
  participant L as ILibraryStore
  U->>P: region drawn (R)
  P->>J: submit job 7 (latest wins)
  J->>L: load library samples
  L-->>J: types with reference images
  J->>M: match(image, region, types)
  M-->>J: candidates: type, pose, score, other types with scores
  J-->>P: immutable snapshot (queued signal)
  P-->>U: components and boxes, plus how many parts match no sample
  Note over P,U: below the score threshold a component is Rejected. The slider filters the cache. It does not search again.
```

這張圖要讓你看到：UI 與 worker 只交換不可變的快照；門檻變動不重新搜尋；庫裡沒有的元件只計數，由使用者決定要不要教新樣本。

## 5b. 樣本庫很大時：少比，不是只靠跑快（ADR-006 §7）

```mermaid
flowchart TD
  R(["Region drawn"]) --> P1["Pass 1: compare with the types the board already uses"]
  P1 --> Q{"Parts left that match nothing?"}
  Q -->|"no"| DONE["Show components. Status bar: N samples compared"]
  Q -->|"yes, scope is auto"| P2["Pass 2: compare the rest of the library"]
  P2 --> DONE
  Q -->|"yes, scope is board types only"| HINT["Hint: use the whole library"]
  HINT -->|"button"| P2
  P1 -.->|"option, not built yet"| PRE["Prefilter by body size in micrometers"]
```

這張圖要讓你看到：第一輪只比板子已經用到的類型（通常很少）；庫裡其他樣本只在需要時才比。預篩選是 S07 要量測後再決定的選項（虛線）。

## 6. Presenter 與介面（給 S04）

Presenter 只依賴窄介面（各 ≤ 5 個方法），View 只呼叫自己需要的 Controller 介面。為了圖面可讀，這裡只畫主幹；完整名單在 `ux-spec.md` 第 12 節。

```mermaid
classDiagram
  class ICanvasController {
    <<interface>>
    +onRegionDrawn(rect, mode)
    +onRegionAborted()
    +onAddRequested(point)
    +onComponentsMoved(ids, delta)
    +onOpenTypeEditor(componentId, boxHint)
  }
  class IProposalController {
    <<interface>>
    +accept()
    +acceptAndSave()
    +discard()
    +cancelSearch()
    +chooseAlternative(componentId, typeId)
  }
  class ITypeEditorController {
    <<interface>>
    +addGroup(kind, rect)
    +updateGroup(groupId, change, scope)
    +addWindow(parentId)
    +addStandardWindows()
    +deleteSelection()
  }
  class ICanvasView {
    <<interface>>
    +showImage(image)
    +showComponents(snapshot)
    +showRegionDraft(rect, mode)
    +setSelection(ids)
    +setInteraction(mode)
  }
  class ITypeEditorView {
    <<interface>>
    +showType(typeSnapshot)
    +showSelection(groupId)
    +showValidation(errors)
    +setDirty(on)
    +close()
  }
  class IStatusView {
    <<interface>>
    +showSummary(summary)
    +showBusy(level)
    +showProgress(fraction)
    +showToast(key, args)
  }
  class AutoRoiPresenter
  class TypeEditorPresenter
  class InteractionStateMachine
  class InspectionDocument
  AutoRoiPresenter ..|> ICanvasController
  AutoRoiPresenter ..|> IProposalController
  TypeEditorPresenter ..|> ITypeEditorController
  AutoRoiPresenter ..> ICanvasView
  AutoRoiPresenter ..> IStatusView
  TypeEditorPresenter ..> ITypeEditorView
  AutoRoiPresenter --> InteractionStateMachine
  AutoRoiPresenter --> InspectionDocument
  TypeEditorPresenter --> InspectionDocument
```

## 7. 復原：三個範圍（ADR-002）

提案和類型草稿各有自己的堆疊；接受、捨棄、儲存類型進文件堆疊。接受與捨棄帶著提案的快照，所以復原能回到預覽。

```mermaid
flowchart LR
  subgraph PS["ProposalSession stack (Preview only)"]
    P1["delete"] --> P2["add one"] --> P3["choose another sample"]
  end
  subgraph TS["TypeEditSession stack (EditingType only)"]
    T1["draw a box"] --> T2["set cols"] --> T3["add all windows"]
  end
  subgraph DS["InspectionDocument stack"]
    D1["move"] --> D2["Accept: carries the proposal snapshot"] --> D3["Save type: before and after"]
  end
  PS -->|"Enter or Esc: redo list cleared first"| DS
  TS -->|"Save: one command, the draft stack is dropped"| DS
  DS -->|"Ctrl+Z on Accept or Discard: UndoToPreview"| PS
```

## 8. 回饋時間與訊息位置

訊息一律在畫布下方的訊息列，不蓋在影像上（使用者回饋後新增）。

```mermaid
flowchart LR
  T0["Press or drag"] -->|"50 ms or less"| T1["Visual feedback"]
  T1 -->|"over 200 ms"| T2["Busy sign in the status bar, stays 400 ms at least"]
  T2 -->|"over 1 s"| T3["Progress bar and Cancel in the message bar"]
  T3 -->|"Cancel"| T4["UI idle in 100 ms or less"]
  T4 --> T5["Late result dropped"]
```

```mermaid
flowchart TB
  TB["Toolbar: Open · Save · Back end · Undo · Redo · Fit · Export"]
  CV["Canvas: image, boxes, region. Nothing is drawn over the parts."]
  MB["Message bar under the canvas: hint · progress · no match · error · toast"]
  DOCK["Docks: Sample library · Board tree · Match parameters (or the type editor panel)"]
  SB["Status bar: back end · ms · components and boxes · cursor · zoom"]
  TB --> CV
  CV --> MB
  CV --- DOCK
  MB --> SB
```

## 9. 取消與「過期結果丟棄」（I4）

UI 不等 worker 停止；worker 的單次比對呼叫無法中途中斷，晚到的結果以工作編號辨識並丟棄。

```mermaid
sequenceDiagram
  actor U as User
  participant P as AutoRoiPresenter
  participant J as Worker (JobRunner)
  U->>P: region drawn
  P->>J: submit job 7
  Note over P: 200 ms: busy sign. 1 s: progress and Cancel.
  U->>P: Esc
  P-->>U: UI idle at once (budget 100 ms)
  Note over P: job 7 is marked stale
  J-->>P: result of job 7 arrives late
  Note over P: the presenter drops it
```
