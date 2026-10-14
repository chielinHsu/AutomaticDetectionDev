# ADR-025 樣本的建立與搜尋分開；結果依位置合併；框由類型編輯（改大小、子框）

- 日期：2026-10-14
- 決定者：使用者提出四個問題並決定做法（2026-10-14 的對話）；Claude Code (Claude Fable 5.1) 整理現況、提出選項與實作
- 狀態：採用

## 背景

使用者在 S11 之後實際用程式建立 PCB 料件的 CAD 框，回報四個問題：

1. 按 S 畫好樣本框就直接搜尋；再畫另一個樣本框又搜尋一次，同一個位置出現多筆重複的 CAD 框。
2. 畫完的 CAD 框不能再調整大小。
3. 要能把整組框一起拖拉移動。
4. 要訓練的樣本是「一個區域」，上面有 N 個 CAD 框；找到之後要把整個區域的框一次貼印到每個找到的位置。

從程式碼確認的現況（S04–S11 的設計）：

- `GenerateObjectsFromSample` 每次掃整張影像、不知道板子上已經有什麼；`InspectionDocument::accept` 把提案直接接在元件後面、每次接受建一個新的暫時類型。所以第二次對同種零件教樣本就把同一批零件再找一遍、再接一次——問題 1 是**跨提案**的重複，不是提案內部的（提案內部已有 NMS 與 `OverlapIndex`）。
- 框由類型推導（ADR-006），接受後的框由 `RoiLayerItem` 批次畫，沒有把手；`RoiGraphicsItem` 的把手幾何只是為類型編輯器準備的，沒有人用。
- `SelectTool` 已經是「拖任何一個已選取的元件，整個選取一起走」；缺框選、全選、選同類型。
- 流程 B 的畫面固定回「樣本庫是空的」；`ILibraryMatcher` 兩個 back end 都有實作但沒有組裝；沒有類型編輯器。

## 決定（使用者 2026-10-14）

### 1. 建立樣本與搜尋分開

- **S 工具放開只建立樣本**：一個新類型（主體框＝樣本框，µm）加上一個樣本元件（中心在框的中心、角度 0、狀態 `Sample`、來源 `Manual`、沒有分數），是文件堆疊上的一個 Command（`AddSample`），狀態回到 `Idle`（新事件 `ReleaseSample`）。畫到一半時有提案的話，提案丟掉（它是用舊的樣本集合搜尋的）。
- **每個樣本自己一個類型，不沿用舊的**。使用者的理由：同種零件在不同光源、髒污、氧化下長得不一樣，「樣本建立是建立，搜尋是搜尋」。
- **〔搜尋〕是另一個動作**（工具列與編輯選單的按鈕、`F5`；新事件 `SearchRequested`：`Idle`、`Preview`、`NoMatch` → `Searching`）：板子上的**每個樣本各跑一次** `GenerateObjectsFromSample`（整張板子、晶格補漏照舊；`SearchBoardWithSamples`），結果**依位置合併**成一份板子的提案（`BoardProposal`，`mergeSampleProposals`）。
- 門檻拖動仍只重新過濾；改角度範圍（`ParamsChanged`）、〔再試一次〕、換 back end 之後的重搜，都是用板子上**現在的**樣本再搜一次。
- 樣本被移動過（拖曳、方向鍵），下次搜尋用新的位置；樣本元件被刪掉，它的類型留著（已有的元件不受影響）、不再參與搜尋。

### 2. 合併的規則（`mergeSampleProposals`）

1. 手動補的元件（來源 `Manual`、不是樣本）的主體框先佔位：和它們重疊的候選剔除（`excluded`）。
2. 各樣本自己（`CandidateSource::Sample`）不放進結果（樣本已經是板子上的元件），但它們的框佔位：找到樣本自己的匹配會被壓掉。
3. 有分數的候選依分數由高到低；和已留下的任何一個重疊超過 `NmsOptions::maxOverlap`（0.5，IoU）的壓掉（`merged`）。
4. 預測的（沒有分數）最後；和已留下的重疊就壓掉。
5. 每個樣本有問題（`sampleProblem`）的不搜尋；全部都有問題時 `sampleProblem` 是第一個問題。

「分數高者取代」只是合併時的排序，成本可以忽略；真正的成本是 N 個樣本各搜尋一次，這是搜尋本身的代價。使用者問「樣本庫越多搜尋越久怎麼辦」：合併不是原因，省時間要靠 ADR-006 §7 的預篩選（P2）。

### 3. 接受＝取代上一次自動產生的元件

`InspectionDocument::accept`：`Found`、`Inferred` 變成元件（指向各自的類型，不再建新類型；`Sample` 已在板子上不再加；`Rejected` 不要）；**上一次自動產生的元件（來源 `Auto`）整批移除**，樣本與手動補的留著。一個 Command（`Accept`），復原回到 `Preview` 並還原提案（ADR-002 規則 1 不變）。所以同一個位置永遠只有一筆。沒有任何可接受的物件時回 0、什麼都不動（留在預覽）。

### 4. 改大小＝改類型（ADR-006 §3 的「此類型全部一起」）

單一元件被選取時畫布顯示把手（`RoiGraphicsItem` 既有的幾何：24 px 點擊區、小框時四個角往外推）；拖把手放開時 `onComponentResized` → `InspectionSession::resizeBody` → `InspectionDocument::setBodySize`：類型的主體框改成新的大小（µm），**同類型全部元件一起變**；拖單邊時中心會移，所以被拖的那個元件的位姿一起改、其他元件的位姿不動。一個 Command（`ResizeType`）。只在 `Idle`（預覽時不顯示把手）。「只改這一個元件」（實例覆寫，ADR-006 未決 #3）仍然延後。

### 5. 整組拖拉：三種都要

- 一個元件的全部框一起走：本來就是（框由類型推導）。
- 框選多個元件一起拖：`SelectTool` 在空白處拖曳＝框選（外框和框相交的元件；Shift＋框選＝加入），沒拖動就是原本的點空白處清除選取。
- 同類型全部：編輯選單〔選取同類型〕（`Ctrl+Shift+A`）；另有〔全選〕（`Ctrl+A`）。

### 6. 區域樣本＋子框（P1，不接樣本庫）

- 樣本可以是一塊區域（搜尋本來就支援，範本就是框內的像素）。
- 新工具 **B**：選取一個元件之後，在它上面框一個子框 → `InspectionSession::addBoxGroup`：換成類型座標（µm、相對主體中心、把元件的旋轉轉回去），存成類型的一個 **Pin 群組（1×1）**，domain 不改；同類型的全部元件立刻多出這個框，之後搜尋找到的也一樣（`expandBoxes`）。一個 Command（`AddBoxGroup`）。
- 預覽也看得到子框：提案的物件帶著類型（`InspectionObject::typeId`），`SceneBuilder` 用類型展開框。
- **P2**（完整流程 B：`Backend` 加比對器、流程 B 的 use case、樣本庫的組裝與面板、參考影像、各層檔案位置）另開 session；R 工具維持現狀（回報樣本庫是空的）。

## 證據

- 事實：`InspectionDocument::accept`（S04 版）不比對既有元件、每次建新類型；`tst_presenter` 的 `aSecondProposalOnTheSameBoardLeavesTheFirstComponentsAlone` 把這個行為當成正確的（它用不同位置的樣本）。
- 事實：`grep` 整個 `src/` 沒有任何地方呼叫 `RoiGraphicsItem::handleAt`／`handleSpots`。
- 事實：`AutoRoiPresenter::finishRegionScan` 固定回 `LibraryEmpty`；`CompositionRoot` 沒有組裝 `ILibraryStore`、`ILibraryMatcher`。
- 量測（這台機器、5 MP、S07–S09 的數字）：OpenCV 每個樣本約 0.1 秒、HALCON 約 0.05 秒再加第一次建模；10 個樣本約 1 秒。合併是 O(n) 的重疊比對，沒有另外量。

## 否決的替代方案

- **搜尋時把既有元件的位置剔除、沿用舊類型**（第一版提案）：使用者否決沿用類型（外觀會變）；剔除改成合併時的規則，而且不再需要「取代既有元件」的 Command（接受本來就整批換）。
- **S 放開就搜尋、再在提案裡標示「已在板上」**：和「建立是建立、搜尋是搜尋」的要求不合；而且每多一個樣本就多一次搜尋，使用者畫 10 個樣本要等 10 次。
- **只加不換（接受時把新結果加進板子）**：同一個位置可能再出現兩筆，使用者選了整批換。
- **新的框種類（子框）**：要改 domain、展開、物件樹與樣式；Pin 1×1 一行 domain 都不用改，之後要「一排腳位」改同一個群組的列數、欄數就有。
- **在 Preview 也能改大小**：預覽時 `Ctrl+Z` 作用在提案堆疊（ADR-002 規則 5），改類型是文件堆疊上的事，兩邊會混在一起；改成只在 `Idle`（建完樣本、接受之後都是 `Idle`）。

## 後果

- 狀態機多兩個事件（`ReleaseSample`、`SearchRequested`），`ux-spec` §3、§6、§8 更新；「三個動作教一個元件」變成「畫樣本、按 F5、按 Enter」。
- `ProposalSession` 持有 `BoardProposal`；`InspectionObject` 多 `typeId`；`CommandKind` 多 `AddSample`、`ResizeType`、`AddBoxGroup`。
- 工具：`SelectTool` 多改大小與框選；新 `BoxTool`（`ToolId::Box`、`DraftMode::Box`）；`ToolEvent` 多把手的資訊；`IToolHost::componentsIn`。
- 介面：`ICanvasController::onComponentResized`、`ISelectionController::selectAll`／`selectSameType`、`IProposalController::search`。
- A/B 比較（S09）一次只比一個樣本：用這次搜尋的第一個樣本。
- 效能量測 T5（放開滑鼠 → 提案就緒）改成「按 F5 → 提案就緒」（`perf_e2e`、`perf_ui_s04`）；基準線要重建（`perf.ps1 -Runs 3 -WriteBaseline`）。
- ux-spec 的「樣本框到多個元件會提示改用 R」仍成立（`SampleCoversMany` 由 back end 判斷）；區域樣本是刻意的用法時，使用者可以忽略那個提示。

## 後續（2026-10-14）：選取資訊表單與樣本庫面板

使用者用了上面的版本之後回報：編輯只能拉框，要能**明確看到並用數字改**樣本的長寬，讓幾個樣本的尺寸可以調成一致；而且建立樣本時要**有地方看到樣本的圖**。使用者確認的做法（分支 `feat/selection-info-panel`）：

1. **選取資訊表單**（ux-spec §4 預留的位置：物件樹下方，`ObjectsPanel` 實作 `ISelectionInfoView`）。選到一個元件時顯示名稱與類型（只顯示）、中心 X、Y（可改＝移動這一個元件，走 `moveSelection`）、寬、高（可改＝改類型的主體框、**中心不動**、同類型一起，走新的 `InspectionSession::resizeBodyKeepingCenter` → 既有的 `resizeBody`／`setBodySize`；有校正值時旁邊顯示 µm）、角度、框數。**按 Enter 或離開欄位才生效**（`QDoubleSpinBox` 的 `editingFinished`，`keyboardTracking` 關；值沒變不送出），一次輸入一個 Command（ADR-002 規則 7）；< `minSamplePx` 不收、吐司、表單的數字回到實際的值。〔套用大小到所有樣本〕＝新的 Command **`ResizeTypes`**（`InspectionDocument::setBodySizeForTypes`：每個類型記「前」的大小，復原各自還原；已經一樣的略過；全部一樣就不進堆疊）。〔鎖定寬高比〕只是表單的行為。多選時表單停用、只顯示「已選 N 個」；預覽中的物件只能改 X、Y（改大小只在 `Idle`，§4 的理由不變）。
2. **樣本庫面板**（`SamplePanel` 實作 `ISampleListView`，放進主視窗原本空殼的 `libraryDock`）：列出板子上的樣本（`InspectionDocument::samples`），每一筆是從目前影像裁下來的縮圖（`SampleListBuilder`：主體框外加 25% 的邊界，`subView` 零複製裁下、`ImageItem::wrap` 換到 8 bit 的只有那一小塊、縮到 96 px 寬、疊上類型展開的框）、名稱、類型、寬 × 高（像素；有校正值時加 µm）、框數。點一筆＝`ISelectionController::select`。清單跟著文件的修訂號走（`SampleListCache`：修訂號與影像沒變就是同一份，View 略過），所以建立、刪除、改大小、加子框、復原都即時更新，門檻拖動不重做。資料只來自板子；**不讀寫樣本庫檔案**（存檔是 P2）。「樣本庫是空的」的文字照舊。
3. **規則在 application 層**（ADR-011）：能不能改位置、改大小由 `InspectionSession::selectionInfo()`（`SelectionInfo`）決定；最小尺寸、預覽中不改大小、多選都在那裡；Presenter 只做吐司與重新發布。`ProposalSession` 多 `poseOf(id)`：只算那一個物件的位姿（表單每次選取、每次門檻變動都要問；`objects()` 是掃全部）。
4. 介面照 ux-spec §12 的做法用窄介面：`ISelectionInfoView`（1 個方法）、`ISampleListView`（2 個）、`ISelectionEditController`（3 個：`setSelectionCenter`、`setSelectionBodySize`、`applySizeToAllSamples`）。

否決：把「套用到所有樣本」做成 N 次 `setBodySize`（N 個 Command，復原要按 N 次）；把選取資訊放在樣本庫 dock（它是元件的資訊，不只樣本的；ux-spec §4 本來就放在物件樹下方）；縮圖用整張影像 `wrap` 再裁（12 bit 的影像每次都要轉整張，5 MP 約 5 ms，裁一小塊才轉只要微秒）。
