# AI 工作紀錄

> 只新增，新紀錄放最上方。每個 agent session 一筆，讓下一位 agent 知道上一位做過什麼、為什麼。
> agent 欄位寫實際的工具與模型；不要從舊紀錄抄。

## 格式

```markdown
## YYYY-MM-DD — Claude Code (模型) ／ Codex (模型)

### 完成
- 做了什麼（一行一件）

### 修改的檔案
- path/to/file

### 驗證
- 實際執行的指令與結果；沒執行就寫「未驗證」與原因

### 未完成
- 還沒做完的事

### 注意事項
- 下一位 agent 需要知道的限制、否決的做法、不要還原的修改
```

## 2026-10-15 — Claude Code (Claude Fable 5.1)（CAD 框吸附到陣列的格點：ADR-026）

### 完成
- 分支 `feat/lattice-snapped-positions`（從 `main` 97495d3 開；session 在 worktree，`main` 在主 checkout 簽出，合併用 `git -C`）。使用者回報「CAD 框與鄰近的框沒有對齊，CAD 框的本質是料件的理論位置」；從程式碼確認只有 `Predicted` 在格點上、契約測試量的是匹配位置；三個做法給使用者比較，使用者選了建議的做法（ADR-026）。
- domain／application：`Candidate`、`BoardCandidate` 多 `latticePose`、`latticeDeviationPx`；`PositionMode`；use case 第 6 階段填晶格位姿（`pose` 不改）；`classify`、`ProposalSession`（`positionMode`、`latticeOf`、`offLattice`）、`InspectionSession::setPositionMode`、`SelectionInfo` 的四個欄位。
- presentation：參數面板「位置」下拉（`IParamsController::setSnapToLattice`、`IParamsSink::applySnapToLattice`、`ParamsState::snapToLattice`）、選取資訊表單「位置」列、預覽卡片的「不在陣列的格點上」；`copy.json` 新增 13 個 key。
- 文件：ADR-026、`docs/decisions.md`、`docs/ux/ux-spec.md`（修改紀錄、§8 兩列）、`docs/architecture.md`、`docs/current-status.md`、本檔、`.ai/handoff.md`。

### 修改的檔案
- `src/domain/match.h`；`src/application/usecases/{GenerateObjectsFromSample.cpp, BoardProposal.h, BoardProposal.cpp, ProposalView.h, ProposalView.cpp}`；`src/application/document/{ProposalSession.h, ProposalSession.cpp}`；`src/application/interaction/{InspectionSession.h, InspectionSession.cpp}`
- `src/presentation/presenters/{ControllerInterfaces.h, ViewInterfaces.h, ParamsPresenter.h, ParamsPresenter.cpp, AutoRoiPresenter.h, AutoRoiPresenter.cpp, SceneBuilder.h, SceneBuilder.cpp, MessageTexts.cpp}`；`src/presentation/params/{ParamsPanel.h, ParamsPanel.cpp, ParamsPanel.ui}`；`src/presentation/objects/{ObjectsPanel.h, ObjectsPanel.cpp, ObjectsPanel.ui}`；`src/presentation/shell/MainWindow.cpp`；`docs/ux/copy.json`
- 測試：`tests/core/{ProposalTestSupport.h, tst_generate_objects.cpp, tst_proposal_session.cpp, tst_session.cpp}`；`tests/ui/{tst_presenter.cpp, tst_params.cpp, tst_messages.cpp}`
- 文件：`docs/adr/026-lattice-snapped-positions.md`、`docs/decisions.md`、`docs/ux/ux-spec.md`、`docs/architecture.md`、`docs/current-status.md`、`docs/changelog-ai.md`、`.ai/handoff.md`

### 驗證
- `scripts/dev/test.ps1`（Release＋OpenCV＋HALCON）：56／57，唯一失敗是 `tst_cli` 的兩個 HALCON 授權提醒項目（等 8／6 秒讀 stderr，機器忙時程式還沒印出），`-NoBuild -Filter 'tst_cli'` 單獨重跑通過（17／17）；有跳過項目的 5 個測試與 2 個 XFAIL 和改之前相同，HALCON 0 跳過。
- `scripts/dev/test.ps1 -Config Debug`：55／57；`tst_cli` 單獨重跑通過；`tst_presenter` 只有 `aSlowSearchShowsBusyThenProgressAndEscCancelsToIdleAtOnce` 失敗（82 passed、1 skipped），單獨重跑四次都一樣——另一個 session 的建置與效能量測讓機器滿載（整組 281–292 秒，正常約 35 秒）；按〔搜尋〕到事件迴圈只有 22–24 ms，是忙碌（40 ms）與進度（150 ms）兩個計時器在 UI 執行緒被餓到時同一輪一起觸發；同一項在 Release 單獨跑通過。**Debug 是 PARTIALLY VERIFIED**，機器安靜時再跑 `-Config Debug -NoBuild -Filter 'tst_presenter'`。
- `node tools/ste-lint.mjs`：979 sentences、0 errors；`node tools/check-deps.mjs`：220 files、0 violations。
- `-Perf`：沒有跑（基準線的重建等使用者指示）。

### 未完成
- Debug 的 `tst_presenter` 那一項沒有在安靜的機器上驗過（見上）。
- 樣本自己的偏差沒有顯示；跨樣本不對齊；位置模式不存檔；畫布上沒有標偏離陣列的；效能基準線沒有重建（使用者另外指示）。

### 注意事項
- `Candidate::pose` 一定要留匹配位置：契約測試（jitter 2 px 板子）、T6b、A/B 比較都看它；吸附在 `classify`／`ProposalSession` 做。
- 「吸附」的門檻就是晶格的內點容差（`LatticeOptions::inlierFraction` 0.25 × 格距），沒有另外的數字；不是內點的保留匹配位置。
- 同一個 session 另有「操作步驟提示與影片操作提示」在它自己的 session 進行（分支待定、ADR 用 027）；它會動 `MessageBar`、`MainWindow`、`copy.json`，合併時以先合併的為準。

## 2026-10-14 — Claude Code (Claude Fable 5.1)（選取資訊表單與樣本庫面板：ADR-025 的「後續（2026-10-14）」）

### 完成
- 分支 `feat/selection-info-panel`（從 `main` 600c757 開；session 在 worktree，`main` 在主 checkout 簽出，合併用 `git -C`）。使用者回報「編輯只能拉框，要能用數字改長寬、讓幾個樣本一致；建立樣本時要看得到樣本的圖」，做法由使用者確認，記在 ADR-025 的「後續（2026-10-14）」。
- application：`InspectionDocument::setBodySizeForTypes`（新 Command `ResizeTypes`：幾個類型的主體框設成同一個大小、位姿不動、各自記「前」；已經一樣的略過、全部一樣不進堆疊）；`InspectionSession` 多 `SelectionInfo`／`selectionInfo()`（規則：`Idle` 能改位置與大小、預覽只能改位置、多選只有數量）、`resizeBodyKeepingCenter`、`applyBodySizeToSamples`；`ProposalSession::poseOf`（只算一個物件）。
- presentation：`ObjectsPanel` 多選取資訊的表單（`ISelectionInfoView`：名稱與類型、中心 X／Y、寬 × 高、µm、角度、框數、〔鎖定寬高比〕、〔套用大小到所有樣本〕；`QDoubleSpinBox` 的 `editingFinished` 才送出、值沒變不送、不收時數字回到實際的值）；新 `library/SamplePanel`（`ISampleListView`，放進主視窗的 `libraryDock`，原本的 `libraryFilter`、`libraryEmpty` 搬進它；點一筆 ＝ `ISelectionController::select`）；新 `presenters/SampleListBuilder`（`subView` 零複製裁下、只轉那一小塊、縮到 96 px、疊上框；`SampleListCache` 依文件修訂號與影像判斷）；`AutoRoiPresenter` 實作新的 `ISelectionEditController`（3 個方法），`publishSelection` 多表單與清單的選取、`publishScene` 多 `publishSamples`；`MainWindow` 多 `samplePanel()`、`Controllers::selectionEdit`。
- 文案：`copy.json` 新增 20 個 key（`ui.selection.*`、`ui.resize.appliedAll`、`ui.resize.nothingToApply`、`ui.history.resizeTypes`、`ui.library.boardSamples`、`ui.library.sample.*`）。
- 文件：ADR-025 加「後續（2026-10-14）」、`docs/decisions.md` 的索引、`ux-spec.md` §4（實作的部分）與 §8（四列）、`docs/architecture.md`、`docs/current-status.md`。

### 修改的檔案
- `src/application/document/{UndoStack.h,InspectionDocument.h,InspectionDocument.cpp,ProposalSession.h,ProposalSession.cpp}`、`src/application/interaction/InspectionSession.{h,cpp}`
- `src/presentation/presenters/{ViewInterfaces.h,ControllerInterfaces.h,AutoRoiPresenter.h,AutoRoiPresenter.cpp,AppPresenters.cpp,MessageTexts.cpp,SampleListBuilder.h,SampleListBuilder.cpp}`、`src/presentation/objects/ObjectsPanel.{h,cpp,ui}`、`src/presentation/library/SamplePanel.{h,cpp,ui}`（新）、`src/presentation/shell/MainWindow.{h,cpp,ui}`
- `AiInspTool/src/presentation/presentation.pro`、`docs/ux/copy.json`
- `tests/core/{tst_document,tst_session}.cpp`、`tests/ui/{tst_messages,tst_shell,tst_presenter}.cpp`
- `docs/adr/025-sample-creation-separate-from-search.md`、`docs/decisions.md`、`docs/ux/ux-spec.md`、`docs/architecture.md`、`docs/current-status.md`、`.ai/handoff.md`、本檔

### 驗證
- `scripts/dev/test.ps1`（Release＋OpenCV＋HALCON）：57／57 通過；有跳過項目的測試（5 個）與 XFAIL（2 個）的清單和改之前相同，HALCON 0 跳過。
- `scripts/dev/test.ps1 -Config Debug`：57／57（`tst_presenter` 82 passed、1 skipped ＝ 截圖）。
- `node tools/ste-lint.mjs`：979 sentences、0 errors；`node tools/check-deps.mjs`：220 files、0 violations。
- `-Perf`：沒有跑（基準線上一個 session 已經要重建）。
- 踩到的坑：拖出來的樣本框寬是 60.0000001（視埠座標換算），清單的文字一度印成「60.0 × 32.0 px」；`SamplePanel` 的格式化改成接近整數就不印小數。

### 未完成
- 樣本庫檔案的讀寫、篩選、〔編輯〕〔刪除〕（P2）；表單的角度只顯示；實例覆寫延後。

### 注意事項
- 縮圖只在文件的修訂號或影像變了才重做（`SampleListCache`）；門檻拖動是同一份指標，View 略過。
- 表單用 `editingFinished`、`keyboardTracking` 關：打字時不送出。〔鎖定寬高比〕靠 `valueChanged` 跟著改另一邊，只是表單的行為。

## 2026-10-14 — Claude Code (Claude Fable 5.1)（CAD 樣本建立的四個問題：樣本的建立與搜尋分開，ADR-025）

### 完成
- 分支 `feat/cad-sample-editing`（從 `main` 4986070 開）。先從程式碼確認四個問題的現況與根因（重複是跨提案的：`accept` 不比對既有元件、每次建新類型、use case 不知道板子；框由類型推導、把手只有幾何沒人用；`SelectTool` 已有整組拖曳但沒有框選；流程 B 沒接），畫現況流程圖，提出選項，使用者決定做法（[ADR-025](adr/025-sample-creation-separate-from-search.md)）。
- **application**：新增 `usecases/BoardProposal`（幾個樣本的結果依位置合併：手動元件佔位先剔除、樣本自己佔位、有分數的依分數由高到低、重疊 > 0.5 壓掉、預測的最後）、`usecases/SearchBoardWithSamples`（每個樣本各跑一次 `GenerateObjectsFromSample` 再合併，進度 (i + f) ÷ n）；`ProposalSession` 改持有 `BoardProposal`（`addManual` 帶類型與大小、`typeOf`）；`InspectionDocument` 多 `addSample`（一個類型＋一個樣本元件）、`setBodySize`、`addBoxGroup`、`samples()`、`componentsOfType`、`bodySizePx`，`accept` 改成「上一次自動產生的整批換掉、樣本與手動的留著、不建新類型」；狀態機多 `ReleaseSample`（`DrawingRegion` → `Idle`）與 `SearchRequested`（`Idle`、`Searching`、`Preview`、`NoMatch` → `Searching`）；`InspectionSession` 多 `startSearch`、`resizeBody`、`addBoxGroup`、`selectAll`、`selectSameType`，`endRegion(Sample)` 回 `SampleAdded`；`CommandKind` 多 `AddSample`、`ResizeType`、`AddBoxGroup`；domain 的 `InspectionObject` 多 `typeId`。
- **presentation**：`SelectTool` 多拖把手改大小（角改兩邊、邊改一邊）與空白處框選（Shift 加入）；新 `BoxTool`（`B`，`DraftMode::Box`）；`ToolEvent` 多把手資訊、`IToolHost::componentsIn`；`ImageCanvas` 多把手 Item（單一選取、工具沒暫停時）；`ICanvasController::onComponentResized`、`ISelectionController::selectAll`／`selectSameType`、`IProposalController::search`；`MainWindow` 多〔搜尋〕（`F5`，上方工具列與編輯選單）、〔全選〕、〔選取同類型〕、`B` 工具；`AutoRoiPresenter` 的搜尋改走 `searchBoardWithSamples`，S 放開建立樣本後切回選取工具並用吐司說「F5 搜尋」，Idle 有樣本時提示換成 `ui.sample.hintIdle`，樣本不再留拖曳中的框（只有區域掃描留）；`SceneBuilder` 的提案物件用類型展開框（子框在預覽就看得到）、`SceneCounts` 多 `merged`／`excluded`／`replaced`，預覽卡片多三行說明；`MessageBar::cardAdvice()`（測試用）。`copy.json` 新增 17 個 key、改 `ui.idle.hint`、`ui.sample.hint`。
- **測試**：`tst_state_machine` 表 47 列；`tst_session`、`tst_document`、`tst_proposal_session`、`tst_scene` 改成新流程並各加新測試（建立樣本、搜尋需要樣本、合併後接受取代、改大小、子框、全選／同類型、`addSample`／`setBodySize`／`addBoxGroup` 的復原）；`tst_tools` 加把手、框選、`B` 工具；`tst_presenter` 加 F5 流程、兩個樣本同一片網格合併且接受不疊加、拖把手改大小、`B` 貼印、框選＋`Ctrl+A`＋同類型；`tst_messages`、`tst_shell` 跟著更新；`perf_ui_s04`、`perf_e2e` 改成樣本建一次、每回合按 F5。
- 文件：ADR-025、`decisions.md`、`ux-spec.md`（§3、§6、§8）、`architecture.md`、`AI_CONTEXT.md`、`current-status.md`、S12 計畫的提醒。

### 修改的檔案
- `src/domain/match.h`；`src/application/`（`document/InspectionDocument.*`、`document/ProposalSession.*`、`document/UndoStack.h`、`interaction/InspectionSession.*`、`interaction/InteractionStateMachine.*`、`usecases/BoardProposal.*`（新）、`usecases/SearchBoardWithSamples.*`（新）、`usecases/ProposalView.*`）；`AiInspTool/src/application/application.pro`
- `src/presentation/`（`canvas/ICanvasController.h`、`canvas/ImageCanvas.*`、`canvas/OverlayTypes.h`、`canvas/RoiGraphicsItem.cpp`、`canvas/RoiLayerItem.*`、`presenters/AutoRoiPresenter.*`、`presenters/ControllerInterfaces.h`、`presenters/MessageTexts.cpp`、`presenters/SceneBuilder.*`、`shell/MainWindow.*`、`shell/MainWindow.ui`、`shell/MessageBar.*`、`tools/ITool.h`、`tools/ToolId.h`、`tools/ToolRegistry.cpp`、`tools/Tools.*`）
- `tests/core/`（`ProposalTestSupport.h`、`tst_document.cpp`、`tst_proposal_session.cpp`、`tst_session.cpp`、`tst_state_machine.cpp`）、`tests/ui/`（`PresenterRig.h`、`TestSupport.h`、`perf_ui_s04.cpp`、`tst_messages.cpp`、`tst_presenter.cpp`、`tst_scene.cpp`、`tst_shell.cpp`、`tst_tools.cpp`）、`tests/perf/perf_e2e.cpp`
- `docs/adr/025-sample-creation-separate-from-search.md`（新）、`docs/decisions.md`、`docs/ux/ux-spec.md`、`docs/ux/copy.json`、`docs/architecture.md`、`docs/current-status.md`、`docs/plan/S12-release.md`、`AI_CONTEXT.md`、`docs/changelog-ai.md`、`.ai/handoff.md`

### 驗證
- `scripts/dev/test.ps1`（預設 Release＋OpenCV＋HALCON）：**57／57 通過**；有跳過項目的測試：`tst_backend_registry`、`tst_shell`、`tst_presenter`（`screenshotsForReview`，要 `AOI_SCREENSHOT_DIR`）、`tst_opencv_image`、`tst_image_sources_agree`（和改之前相同）；XFAIL 2 列和之前相同（opencv B1、`tst_image_sources_agree` 的 1 列）。`tst_tools` 的三個新測試在這次之後才加，單獨重跑的結果見交接單。
- `test.ps1 -Config Debug`：**57／57 通過**（log 2 362 項 passed、0 failed）；`test.ps1 -Filter tst_tools`（含三個新測試）：Totals 38 passed。
- `node tools/ste-lint.mjs`：979 sentences、0 errors；`node tools/check-deps.mjs`：216 files、0 violations。
- 效能測試（`-Perf`）與基準線**沒有跑**：T5 的定義變了（按〔搜尋〕→ 提案就緒），基準線要重建。

### 未完成
- P2（完整流程 B：`Backend` 加比對器、流程 B 的 use case、樣本庫的組裝與面板、參考影像、各層檔案位置）：使用者決定另開 session。
- 刪除單一子框（目前只能 `Ctrl+Z`）、Pin 矩陣的列數欄數 UI、只改這一個元件（ADR-006 未決 #3）。
- 效能基準線重建（`perf.ps1 -Runs 3 -WriteBaseline`）。

### 注意事項
- `accept` 的語意變了：上一次自動產生的元件（來源 `Auto`、不是樣本）整批換掉。`ProposalSession` 的 Sample 物件不再被接受（樣本已經在板子上）；合併過的提案根本不含樣本自己。
- 樣本元件是 `state = Sample`、`origin = Manual`、沒有分數；它的框由類型的主體框決定，所以改大小會讓 `samples()` 的框跟著變、下次搜尋用新的框。
- 預覽時不顯示把手、不能改大小（ADR-002 規則 5 的堆疊路由）。
- 一個 commit 放不下「一個意圖」的切法：`ToolEvent`、`ICanvasController`、`ISelectionController` 的介面變更同時被四件事用到，拆開會有建不起來的中間 commit；所以程式一個 commit、文件一個 commit。

## 2026-10-13 — Claude Code (Claude Fable 5.1)（S11：效能優化，先量再改）

### 完成
- 分支 `step/11-optimize`（從 `main` 98db42a 開），`--no-ff` 合併到 `main`，tag `step-11-done`，沒有 push。
- 開工時確認 `tst_contract_end_to_end` 28／28、HALCON 0 跳過、XFAIL 只剩 opencv B1（D6）。
- **挑選**（S10 報表 2026-10-13 16:46 那次）：唯一超出預算的指標是 U3（p95 26.7 對 16）；互動與搜尋類都在預算內，所以只做 U3；搜尋類列成候選、沒有改（perf.md §5）。
- **先量**：`perf_ui_s04` 加 `U3-canvas-paint-image`／`-layer` 診斷（一次重繪 14.2 ms：影像 1.1、框層 12.5）；`perf_paint_micro` 加字形 blit 的案例（2 600 個框 5.13 → 0.35 ms；dpr 2 的情況 2.31 ms）。
- **優化 1**（`ff1f262`，[ADR-024](adr/024-box-glyph-cache.md)）：`RoiLayerItem` 小框用字形快取畫——每種（狀態、種類、螢幕寬高）用同一套 `BoxFrames` 畫一張 (w+2)×(h+2) 的 ARGB 小圖，每個框一次未轉換的 `drawImage`；`BoxFrames::snap` 是共用的取整規則；dpr ≠ 1、框超過 96 px、選取的框照舊；快取 64 張。框層 12.5 → 1.15 ms；U3 18.9／23.5 → 13.5／17.6 ms；I1 p95 8.1 → 4.5；I3 22.9 → 7.1。`tst_items` 加三個測試（四種狀態 × 三種框、三種縮放逐像素相同；dpr 2 不用字形；快取命中）。
- **優化 2**（`a92ac95`）：`ComponentView` 的 `label` 改成 `text` ＋ `score`，`label()` 畫的時候才組（`RoiLayerItem` 只對真的要畫的標籤格式化）；提案列的 `ObjectRow.name` 留空，`displayName()` 顯示時組「#編號」。場景重建 3.46 → 0.89 ms；U3 → 8.75／11.2 ms（**進入預算**）；I6 4.4 → 1.7。
- **優化 3**（`a317131`）：`InspectionDocument::revision()`（每次變更通知加 1；`tst_document` 加測試）＋ `SceneBuilder` 的 `DocumentSceneCache`（鍵 ＝ 修訂號 ＋ 標籤文字；命中時文件那一部分共用上一次的快照與列，沒有提案時連快照都不複製；Presenter 持有一份；`tst_scene` 加測試）。U3-bg（背景 4 883 個已接受的元件）17.8 → 7.2 ms；U3 → 6.5／7.6 ms（差異在飄移內）。
- **預算檔**（`c8511a2`）：`perf/budgets.json` 的 U3 改 `calibrated`、新增 `uiCalibrationS11`（limit 沒有改）；`perf_ui_s04` 的 `knownOver("U3")` 拿掉。
- 文件：四種輸出與 `perf.md`（`docs/steps/S11-optimize/`）、ADR-024、`decisions.md`、`architecture.md`、`AI_CONTEXT.md`、`00-master-plan.md`、S12 計畫的提醒、`current-status.md`、交接單。

### 修改的檔案
- 新增：`docs/adr/024-box-glyph-cache.md`、`docs/steps/S11-optimize/{spec,diagram,storyboard,perf}.md`、`explainer.html`
- 修改：`src/presentation/canvas/{BoxFrames.h,BoxFrames.cpp,RoiLayerItem.h,RoiLayerItem.cpp,ImageCanvas.cpp,ComponentSnapshot.h,RoiGraphicsItem.cpp}`、`src/presentation/presenters/{SceneBuilder.h,SceneBuilder.cpp,AutoRoiPresenter.h,AutoRoiPresenter.cpp}`、`src/presentation/objects/{ObjectRows.h,ObjectsModel.cpp}`、`src/presentation/dev/FakeScene.cpp`、`src/application/document/{InspectionDocument.h,InspectionDocument.cpp}`、`tests/ui/{tst_items,tst_scene,tst_presenter,perf_ui_s04,perf_paint_micro}.cpp`、`tests/core/tst_document.cpp`、`perf/budgets.json`、`perf/baseline/casparhsu1-harness.json`、`AI_CONTEXT.md`、`docs/{architecture,decisions,00-master-plan,current-status,changelog-ai}.md`、`docs/plan/S12-release.md`、`.ai/handoff.md`
- 沒有改：`domain/`、兩個 back end、use case、`ContractTolerances.h`、`perf/budgets.json` 的 limit、`.pro`（沒有新的執行檔）

### 驗證
- `test.ps1` 預設（Release，OpenCV＋HALCON）：**57／57**（HALCON 0 跳過；有跳過的 5 個測試和之前相同；XFAIL 2 列：opencv B1、`tst_image_sources_agree` 的 1 列，和之前相同）。
- `test.ps1 -Config Debug`：**57／57**（HALCON 0 跳過；跳過與 XFAIL 的清單和 Release 相同）
- `node tools/ste-lint.mjs`：979 sentences，0 errors；`check-deps` 的 3 項在 `test.ps1` 裡通過；`scripts/ai/check-ai-structure.ps1`：OK（既有警告：`current-status.md` 超過 80 行）
- 效能：每個優化之後 `test.ps1 -Perf -NoBuild -Filter perf_ui_s04` 各一次、優化 1 之後 `perf_ui_baseline` 一次（數字在 perf.md §3）；`perf.ps1 -NoBuild -Runs 3 -WriteBaseline`：三次都 0 項超出預算、0 個測試失敗；U3 三次中位數 5.8–9.7、p95 6.5–10.9 ms；對舊基準線的閘門三次都 FAIL（改善 42–48 個都是 UI 的指標；標成回歸的 13–61 個都不在這一步改的程式裡，是機器的飄移，第 1 次處理器只有 80.8%）；新基準線 641 個指標（`docs/steps/S11-optimize/perf.md` §6）

### 未完成
- 搜尋類沒有動（都在預算內）；AB-U3 沒有預算、T7 的預算仍等使用者決定；`perf.ps1 -Window` 仍沒有跑過，I1 真實視窗的數字（ADR-009）沒有重量。
- dpr ≠ 1 時字形快取不生效（照舊填色矩形）：高 DPI 的機器上 U3 會回到 S04 的數字。

### 注意事項
- 字形與填色矩形必須產生相同的像素（`tst_items` 的 `glyphsGiveTheSamePixelsAsFilledRects`）：改 `BoxFrames` 時兩條路都會變，改 `RoiLayerItem` 的字形時要讓這個測試過。
- `DocumentSceneCache` 的鍵只有修訂號與標籤：任何會改板子內容、但不經過 `InspectionDocument::notify` 的新路徑都會讓畫面過期——新的文件修改一律做成 Command（本來的規則）。
- U3 的 `knownOver` 已拿掉：這台機器整台變慢時（S10 的第 1 次那種）`perf_ui_s04` 可能因 U3 超標而失敗，看報表上處理器的實際速度再判斷。

## 2026-10-13 — Claude Code (Claude Fable 5.1)（HALCON 準確度的修正：ADR-004 §5 的 B1–B3 與 O1）

### 完成
- 分支 `fix/halcon-accuracy`（從 `main` 76fd732 開），`--no-ff` 合併到 `main`，沒有 tag、沒有 push。
- **先找根因再改**（探測程式 `tests/backend/probe_halcon_accuracy.cpp`、`probe_opencv_angle.cpp`，不列進 `tests.pro`；十一輪量測記在 `docs/steps/S09-contract-ab/halcon-accuracy-probe.md`）。ADR-004 §5 的推測全部不對：B1 是 `create_shape_model` 的 'auto' 對比在「對比 12、雜訊 ±4、模糊 1」的樣本上選到 5／5（雜訊的量級：模型 419 點有 120 點不在邊上，其他元件分數 0.59、連樣本自己 0.71）；B2 是模糊後「電極–底–電極」和模型只差主體上下邊那三成的點（半格距分數 ≈ 1 − 59／349 ＝ 0.83，量到 0.825），570 個重複偵測**全部**是 use case 驗證階段對半格距種子的後果（`dup by verified`）；B3 是 'auto' 把門檻拉到 38、主體長邊掉出模型（114 點只剩 7 點在主體邊上）。O1 是角度 0 的範本帶著樣本自己的像素雜訊、轉過角度的範本被雙線性內插平掉，NCC 對角度的曲線在 0° 凹下去（量到曲線）。
- **修正（都在 `src/infrastructure/backend_halcon`；使用者 2026-10-13 確認後改 `HalconSearchOptions` 的預設）**：（1）MaxOverlap 0.5 → 0.2（半格距候選和兩邊真的元件各重疊 25%；六種板子真的匹配一個都不少）；（2）建範本時量樣本的邊寬（`estimateSampleStatistics`：灰階 p95 − p5 ÷ Sobel 梯度 p99）與相對雜訊（像素和 3×3 平均的差的中位數 × 1.4826 ÷ 灰階範圍）：邊寬 ≥ 5 → 建模型與搜尋之前先 `mean_image` 5×5 三次、SubPixel 'least_squares'、只追蹤到金字塔第 2 層（HALCON 說明對失焦影像的建議）；相對雜訊 ≥ 4% → `mean_image` 3×3 三次、'least_squares_high'；其餘不平滑（零複製、時間和 S08 相同）。決定寫在範本裡（`HalconTemplateTuning`），搜尋影像跟著做；流程 B 不平滑。HALCON 的 `gauss_filter`、`binomial_filter` 同樣的 σ 都比 `mean_image` 三次差（B1 角度 0.92 對 0.75°）；'interpolation' 在模糊的板子不穩（同 σ 的三種濾波器 0.24、0.60、0.91°），不用；第一版「邊寬 ≥ 3 就平滑、全部 least_squares_high」讓一般板子的 T2b 慢一倍、T4 超出預算，所以加了相對雜訊的條件。
- **使用者的四個決定**：HALCON 採用上面這組；「強雜訊」改成相對定義（`ContractTolerances.h` 的 `isStrongNoise`：雜訊 ≥ 邊緣對比的 1/8；數字沒有放寬）；O1 列為接受的差異（ADR-004 的 D6），OpenCV 的 B1 列留 `QEXPECT_FAIL` 當哨兵；OpenCV 在低對比沒有雜訊時的半格距候選只記錄不改。
- 測試：`tst_halcon_backend` 加 2 項（邊寬與雜訊決定平滑、次像素與追蹤的層；重疊超過兩成的兩個實例是同一個）；`tst_contract_end_to_end` 拿掉 HALCON 的三個 `QEXPECT_FAIL`、OpenCV B1 的改成 D6 的說明。`perf/budgets.json` 的 `knownIssues` 清空、`perf_matrix.cpp` 的 `knownIssue()` 拿掉。基準線 `perf/baseline/casparhsu1-harness.json` 重建。
- 文件：ADR-004（狀態、D2 的相對定義、新的 D6、「後續（2026-10-13）」）、ADR-017（後續）、`decisions.md`、`architecture.md`、`AI_CONTEXT.md`（XFAIL 只剩 1 列）、`current-status.md`、交接單。

### 修改的檔案
- 新增：`docs/steps/S09-contract-ab/halcon-accuracy-probe.md`、`tests/backend/{probe_halcon_accuracy,probe_opencv_angle}.cpp`、`AiInspTool/tests/backend/{probe_halcon_accuracy,probe_opencv_angle}.pro`
- 修改：`src/infrastructure/backend_halcon/{HalconSearchOptions.h,HalconTemplate.h,HalconTemplate.cpp,HalconTemplateBuilder.cpp,HalconSearchSteps.h,HalconSearchSteps.cpp,HalconPatternSearcher.h,HalconPatternSearcher.cpp,HalconLibraryMatcher.h}`、`tests/backend/tst_halcon_backend.cpp`、`tests/contract/{ContractTolerances.h,tst_contract_end_to_end.cpp}`、`tests/perf/perf_matrix.cpp`、`perf/budgets.json`、`perf/baseline/casparhsu1-harness.json`、`AI_CONTEXT.md`、`docs/{architecture,decisions,current-status,changelog-ai}.md`、`docs/adr/{004-backend-divergence,017-halcon-shape-model-backend}.md`、`.ai/handoff.md`
- 沒有改：`application/`、`domain/`、OpenCV 的 adapter、`ContractTolerances.h` 的數字、`OpenCvSearchOptions`

### 驗證
- `test.ps1` 預設（Release，OpenCV＋HALCON）：**57／57**（HALCON 0 跳過；XFAIL 只剩 `tst_contract_end_to_end` 的 opencv B1 與 `tst_image_sources_agree` 的 1 列，和修正前比少了 HALCON 的 3 列）；`test.ps1 -Config Debug`：**57／57**。
- `tst_contract_end_to_end` 28／28：HALCON B1 841 全找到、0 誤判、0 重複、位置最大 0.353 px、角度 0.745°（上限 0.801，相對定義）；B2 0 誤判、0 重複、0.215 px、0.575°（上限 0.6）；B3 0.142 px、0.650°（上限 1.031）；其他 24 列照舊通過，數字和修正前相同或更好（雜訊 12 模糊 1：角度 0.898 → 0.463°）。
- `node tools/ste-lint.mjs` 0 errors；`scripts/ai/check-ai-structure.ps1` OK（既有的警告：`current-status.md` 超過 80 行）。
- 效能（`scripts/dev/perf.ps1 -NoBuild`，和修正前的基準線比）：`perf_halcon` **0 項超出預算**：T1 中位數 6.9／p95 7.4 ms（30）、T2b 20.4／24.7 ms（80；基準線的範圍內）、T4 24.7／42.9 ms（60）、T6a 100%、T6b p95 **0.114 px**（修正前 0.485）；閘門只標 C-search 一項（中位數 16.5 對基準線 2.1–12.7，p95 在範圍內），其餘 16 項變快。`perf_ab -Runs 3`：12 種情境兩個 back end 都 100%；HALCON 低對比召回率 0.1% → **100%**（位置 p95 0.189 px）、重模糊誤判 242 → **0**、位置 p95 0.536 → 0.15 px、use case 130–189 → 61–92 ms、峰值 74–85 → 20–28 MB、雜訊 12 模糊 2 位置 p95 0.933 → **0.11 px**。閘門標的「回歸」：低對比的 use case 30–46 → 70–96 ms（以前是什麼都沒找到的快）、峰值 +47 MB（平滑過的影像）、10% 缺件的峰值 22–24 → 56–60 MB（HALCON 的工作記憶體在同一個行程裡累積，ADR-023 說過峰值只能看大的差別）。基準線重建（`perf.ps1 -NoBuild -Runs 3 -WriteBaseline`，每次約 12 分鐘）：637 個指標、3 次的範圍寫進 `perf/baseline/casparhsu1-harness.json`；這 3 次對**舊**基準線的閘門都 FAIL（結束碼 1），0 項超出預算，標的「回歸」是低對比那一格（現在找得到了）與第 2 次整台變慢時的一批（OpenCV 的格子也在內，雜訊；ADR-023 的 INCONCLUSIVE 條件沒到）。

### 未完成
- B2 的角度餘裕只有 4%（0.575 對 0.6）、B1 7%（0.745 對 0.80）：合成板子是確定的，同一個建置每次一樣；換 HALCON 版本要重跑。
- use case 驗證階段的漏洞（視窗裡找到的匹配離別的已知候選更近時仍當成那個缺格的元件）記在 ADR-004 與 `current-status.md` 的待決定（10），沒有改 application。
- 真實影像（HA）沒有重量：新的影像位置要使用者確認（ADR-004 §5 最後一項）。

### 注意事項
- `HalconSearchOptions` 的新預設（MaxOverlap 0.2、adaptiveSmoothing 與它的門檻）是使用者 2026-10-13 確認過的；要再改先看 `halcon-accuracy-probe.md`。
- 探測程式不在 `tests.pro`：要跑時在 `build/qmake-release/tests/backend/<name>/` 用 `qmake <repo>/AiInspTool/tests/backend/<name>.pro AOI_BUILD_ROOT=<build>` 再 `jom`；`AOI_PROBE_SET`、`AOI_PROBE_BOARDS`、`AOI_PROBE_VARIANTS` 選量測。
- 'interpolation'、`gauss_filter`、`binomial_filter`、「全部平滑」都試過、都比現在的差（probe.md §8），不要憑印象改回去。

## 2026-10-13 — Claude Code (Claude Fable 5.1)（S10：效能測試 harness 與回歸閘門）

### 完成
- 分支 `step/10-perf-harness`（從 `main` 37463db 開），`--no-ff` 合併到 `main`，tag `step-10-done`，沒有 push。
- 一鍵流程 `scripts/dev/perf.ps1`：量測當下的環境快照（電源計畫、接電源、CPU 負載、處理器對額定速度的百分比、最忙的行程）→ `test.ps1 -Perf` → `perf-compare collect` → `compare`（→ `.html` 報表）；`-Runs N`（連跑並算雜訊）、`-WriteBaseline`、`-Filter`、`-Window`、`-Profile full`、`-InjectDelayMs`（自我測試）。
- 閘門 `tools/perf-compare.mjs`（[ADR-023](adr/023-perf-regression-gate.md)）：`collect` 把 11 種結果檔（S03–S09 的 `perf_*` 與 S10 的）與測試 log（Totals、SKIP、XFAIL、XPASS）正規化成一份執行紀錄 `perf/results/<日期>-<commit>-<機器>.json`；`compare`（預算看 p95；基準線記範圍、中位數比上緣高超過 15% 才算回歸；已知的只在 `budgets.json`、列出不擋；跳過＝未驗證、XFAIL＝已知待修；一半以上變慢＝INCONCLUSIVE；結束碼 0／1／2／3）；`baseline`、`noise`、`html`、`--self-test`（20 項，也是 `test.ps1` 的第 5 個 node 檢查 `perf_compare_self_test`）。
- 新的量測 `tests/perf/`：`perf_matrix`（搜尋矩陣：4 種影像大小 × 3 種樣本 × 3 種角度 × 3 種元件數 × 6 種難度 × 每個會讀像素的 back end；quick 15 格／full 648 格；時間、四個階段、峰值〔熱機之後〕、召回率、位置誤差；`H-overhead`、`H-noise`）、`perf_e2e`（T5：放開滑鼠 → `searchApplied`，走產品的接線，每個 back end 一列，`PerfProbe` 的切片）、`HarnessSupport.h`；`.pro` 在 `AiInspTool/tests/perf/`，列進 `tests.pro` 的 `AOI_PERF_TESTS`。
- 報表 ＝ `docs/steps/S10-perf-harness/explainer.html`（嵌入第 3 次執行與基準線；篩選、搜尋、拖門檻重算、載入別次的 JSON；在瀏覽器確認過）。基準線 `perf/baseline/casparhsu1-harness.json`（整套跑 3 次，637 個指標的範圍）。
- `perf/budgets.json`：T5 標 `calibrated`（OpenCV p95 28.7–39.2 ms、HALCON 17.5–22.1 ms，預算 300）、頂層 `status` 改 `calibrated`（limit 一個都沒改）、新增 `knownIssues`（ADR-004 §5 的 B1–B3）與 `harnessCalibrationS10`。
- 文件：四種輸出與 `perf.md`、ADR-023、`decisions.md`、`architecture.md`、`perf-test-plan.md`、`AI_CONTEXT.md`（測試數 57、`tests/perf/`、效能閘門的列）、`00-master-plan.md`、S11／S12 計畫的提醒、`current-status.md`、交接單。

### 修改的檔案
- 新增：`tests/perf/{HarnessSupport.h,perf_matrix.cpp,perf_e2e.cpp}`、`AiInspTool/tests/perf/{perf_tests.pri,perf_matrix.pro,perf_e2e.pro}`、`tools/perf-compare.mjs`、`scripts/dev/perf.ps1`、`perf/baseline/casparhsu1-harness.json`、`docs/adr/023-perf-regression-gate.md`、`docs/steps/S10-perf-harness/{spec,diagram,storyboard,perf}.md`、`explainer.html`
- 修改：`AiInspTool/tests/tests.pro`、`scripts/dev/test.ps1`（node 檢查 ＋ 說明）、`perf/budgets.json`、`AI_CONTEXT.md`、`docs/{architecture,decisions,perf-test-plan,00-master-plan,current-status,changelog-ai}.md`、`docs/plan/{S11-optimize,S12-release}.md`、`.ai/handoff.md`
- 沒有改：`src/`、S02–S09 的量測程式、`ContractTolerances.h`、兩個 back end 的預設

### 驗證
- `test.ps1` 預設（Release，OpenCV＋HALCON）：**57／57**（HALCON 0 跳過；`tst_contract_end_to_end` 4 列 XFAIL、`tst_image_sources_agree` 1 列 XFAIL 照舊）。
- `test.ps1 -Config Debug -Filter perf_e2e`（整個 Debug 建置通過，兩個新執行檔也編得過）＋ `test.ps1 -Config Debug -Perf -NoBuild -Filter perf_e2e`：`perf_e2e` 在 Debug 跑完、三個 back end 都有列（Debug 的數字沒有意義）。
- `node tools/perf-compare.mjs --self-test` 20 項通過；`node tools/ste-lint.mjs --strict` 0 errors；`node tools/check-deps.mjs` 在 `test.ps1` 裡通過。
- 效能：整套 `test.ps1 -Perf` 跑 3 次（每次約 15 分鐘）建基準線；第 3 次對基準線 PASS（0 回歸）；`perf.ps1 -NoBuild -Filter perf_matrix -InjectDelayMs 60` 閘門 FAIL（35 個回歸）、`baseline` 拒收。數字在 `docs/steps/S10-perf-harness/perf.md`。
- 跨行程的雜訊中位數 44%（第 1 次整台慢 1.5–2.5 倍，處理器只跑在額定速度的 54–67%）：計畫的「雜訊低於閾值」沒有達成，改以範圍與 INCONCLUSIVE 處理（ADR-023）。

### 未完成
- `-Window`（真機視窗）、`-Profile full`、真實影像（`AOI_REAL_IMAGE_DIR`）沒有跑過。
- 基準線含一次慢的執行，範圍偏寬：機器狀態正常時重建。
- AB-U3、T7 的預算仍等使用者決定；U3、I1 仍是已知。

### 注意事項
- 已知的超標只登記在 `perf/budgets.json`（`status`、`knownIssues`）與 `perf_matrix.cpp` 的 `knownIssue()`；修好要拿掉，否則閘門不會擋。
- 量測時關掉 Visual Studio、Chrome、遠端桌面；報表上處理器的實際速度 < 90% 會警告。
- `perf.ps1` 第一次的 env 檔有 BOM 讓 `collect` 失敗（已修：`readJson` 去 BOM）；`CallNtPowerInformation` 只回報基礎時脈（2 200 MHz），看不到 turbo，所以另外用 `% Processor Performance` 計數器。
- HALCON 第一次處理某個大小的影像配置很大的工作緩衝（5 MP 約 1.1 GB、12 MP 約 1.9 GB），一次性；峰值從熱機之後取樣。

## 2026-10-12 — Claude Code (Claude Fable 5.1)（B1–B3 的回歸測試）

### 完成
- 分支 `test/halcon-accuracy-regression`（從 `main` 41f5a74 開），`--no-ff` 合併到 `main`，沒有 tag、沒有 push。
- `tests/contract/tst_contract_end_to_end.cpp`：`syntheticBoards` 多三列（ADR-004 §5 的 B1 低對比、B2 重模糊、B3 雜訊 12＋模糊 2）。先用原本的 1280×960 板子試：HALCON 的 B1、B2 都 100% 找到、沒有多餘的候選（重現不了），B3 重現（最大 0.709 px）；所以 B1、B2 改用 `perf_ab` 的 5 MP 板子（複製成 `abBoardSpec()`，56×24 的元件、格距 84×60、841 個、雜訊 4＋模糊 1），B3 用原本的。`evaluate()` 多了「重複」的計數（每個有元件的格子最多一個有分數的候選），套在所有列；既有 16 列照樣通過，沒有新的發現。
- `QEXPECT_FAIL`（Abort）4 列，放在第一個會失敗的檢查之前：HALCON B1「全部找到」（841 個只找到樣本、沒有晶格）、HALCON B2「沒有誤判」（242 個誤判、570 個重複、位置最大 0.754 px、角度最大 3.3°）、HALCON B3「位置 ≤ 0.5 px」（最大 0.709 px；角度最大 2.4°）、**OpenCV B1「角度」**——O1：841 個都找到、位置最大 0.173 px，但角度最大 1.200°（上限 0.6°）。O1 是加測試時發現的（`perf_ab` 沒量角度）；探測：拿掉雜訊或拿掉模糊都在 0.13° 內，是低對比加上雜訊 4＋模糊 1 一起才會。**使用者決定**：OpenCV 這一列一樣標 XFAIL，和 B1–B3 一起交給修正的 session。板子的名稱用巨集（`AOI_BOARD_B1` …），讓 `_data` 與 `QEXPECT_FAIL` 的列名是同一個字串。
- 反向確認：拿掉 B3 的標記，那一列真的 FAIL（27 passed、1 failed），放回去恢復。
- `scripts/dev/test.ps1`：每個測試讀 log 的 `XFAIL` 行，最後列出「N 個測試有預期失敗（XFAIL）的項目（已知、待修）」與列名；失敗時的摘要也印 `XPASS` 行。順帶讓既有的一個 XFAIL 浮出來：`tst_image_sources_agree` 的 `bothSourcesGiveTheSameImage(palette BMP, 16 grays)`（S06 就有，沒有動）。
- 探測時看到、沒有加測試（ADR-004 §5）：低對比的 5 MP 板子沒有雜訊時，HALCON（雜訊 0 或 4、模糊 0）與 OpenCV（雜訊 0、模糊 1）都在半個格距多出幾百個候選——B2 那種誤判不只 HALCON、不只重模糊。
- **HALCON dongle 第五次讀不到**（跑完整測試途中；重插兩次都沒用）。使用者裝了 CodeMeter Runtime 9.10a 之後，log 寫出「偵測到 dongle 時讀到 Box Event → 限制 5 分鐘」；5 分鐘一到 HALCON 就正常。之後的實驗（合併後的小 commit）：完整的 Release＋Debug 沒有再出現 Box Event；再拔插一次也沒有（HALCON 立刻能用）——重插與正常流程都不會觸發，事件是 dongle 內部先前記錄的，原因仍不明。查證網路上「`get_system('dongle_ids')`／`'license_info'`」的說法：24.11 的 `operators_en_US.ref` 沒有這兩個參數。都寫進 AI_CONTEXT 的 HALCON 那一列。
- 文件：ADR-004（狀態、§5 的回歸測試／O1／探測）、AI_CONTEXT（`tests/contract` 列、測試說明、`VERIFIED` 的定義、詞彙表加 XFAIL、HALCON 列）、current-status、decisions（ADR-004 那一列）、`docs/plan/S10-perf-harness.md` 提醒第 7 點、`docs/plan/S11-optimize.md` 修正 session 的 Start prompt（加 O1 與探測的現象）、交接單。

### 修改的檔案
- tests/contract/tst_contract_end_to_end.cpp、AiInspTool/tests/contract/tst_contract_end_to_end.pro
- scripts/dev/test.ps1
- docs/adr/004-backend-divergence.md、AI_CONTEXT.md、docs/current-status.md、docs/decisions.md
- docs/plan/S10-perf-harness.md、docs/plan/S11-optimize.md
- docs/changelog-ai.md、.ai/handoff.md

### 驗證
- 加列之後的探測（Release、HALCON 讀得到時）：`tst_contract_end_to_end` 兩個 back end各 8＋3 列的實際數字見 ADR-004 §5。
- 標記完成後：`test.ps1 -Filter 'tst_contract_end_to_end'` 28 passed、0 failed、0 skipped、4 XFAIL；反向確認 1 次。
- `test.ps1` 預設（Release，第一輪）：56／56 但 HALCON 在途中讀不到（HALCON 的列與測試 SKIPPED）；dongle 恢復後 `-NoBuild` 重跑：**56／56、HALCON 0 跳過**，XFAIL 列表 ＝ `tst_contract_end_to_end` 4 列＋`tst_image_sources_agree` 1 列（剩下 5 個有跳過的測試是 S09 之前就有的同一組項目）。
- `test.ps1 -NoHalcon`：**54／54**，HALCON 的列 SKIPPED、OpenCV 的 B1 XFAIL、B2／B3 PASS。
- `test.ps1 -Config Debug`：**56／56**，HALCON 0 跳過，XFAIL 列表和 Release 相同（`tst_contract_end_to_end` 4 列＋`tst_image_sources_agree` 1 列；Debug 的 `tst_contract_end_to_end` 27.9 秒）。
- `node tools/ste-lint.mjs --strict`：899 sentences, 0 errors。

### 未完成
- 無。B1–B3＋O1 的修正是另一個 session（S10 之後）。

### 注意事項
- 不要為了讓 XFAIL 的列通過而放寬 `ContractTolerances.h`；修好那一列才拿掉它的 `QEXPECT_FAIL`。
- B1、B2 的板子是 5 MP：HALCON 的 B2 那一列約 150 ms、整個測試執行檔約 5–7 秒。
- HALCON 讀不到時先等 5 分鐘再試，不要一直重插（見 AI_CONTEXT）。

## 2026-10-12 — Claude Code (Claude Opus 5.5)（使用者對 HALCON B1–B3 的決定）

### 完成
- 記下使用者的決定（ADR-004 §5）：（1）S09 之後先不修；（2）在 S10 之後、S11 之前另開 session 修（分支 `fix/halcon-accuracy`）；（3）先把三種板子加進 `tst_contract_end_to_end` 當回歸測試，HALCON 的列用 `QEXPECT_FAIL` 標成預期失敗（使用者從「預期失敗」與「真的失敗」選了前者：真的失敗會讓 `test.ps1` 一直 55／56，S10 起每一步都達不到 VERIFIED）。
- `docs/plan/S11-optimize.md` 開頭加「S11 之前先做」與修正那個 session 的 Start prompt；`依賴` 加上這個修正。`docs/plan/S10-perf-harness.md` 的提醒加第 7 點（S10 不修；XFAIL 在回歸閘門裡怎麼看；S10 收工時開修正的 session，不是 S11）。
- `current-status.md`、`decisions.md`、交接單：接下來的順序是回歸測試 → S10 → 修正 → S11。
- 回歸測試本身沒有在這個 session 做（使用者的習慣：確認之後開新 session 實作）；用 `spawn_task` 開了回歸測試的 session，它收工時開 S10。原本的 S10 卡片撤掉。

### 修改的檔案
- docs/adr/004-backend-divergence.md
- docs/plan/S10-perf-harness.md、docs/plan/S11-optimize.md
- docs/current-status.md、docs/decisions.md
- docs/changelog-ai.md、.ai/handoff.md

### 驗證
- 只改文件，沒有建置。`node tools/ste-lint.mjs --strict`：899 句，0 error、0 warning。

### 未完成
- 回歸測試（新 session）、S10、B1–B3 的修正、S11，照上面的順序。

### 注意事項
- `QEXPECT_FAIL` 要放在 HALCON 那幾列實際失敗的那一個檢查之前（跑過才知道是哪一個）；放錯會變成真的 FAIL 或 XPASS。

## 2026-10-12 — Claude Code (Claude Opus 5.5)（S09 的 HALCON 補測）

### 完成
- 使用者告知 dongle 恢復，照 `docs/steps/S09-contract-ab/perf.md` §6 在 `main`（b60592b，S09 合併之後）補做 HALCON 的部分；程式沒有改。分支 `chore/s09-halcon-verification`。
- 契約測試的 HALCON 列在最後一版的程式上通過，數字和 2026-10-09 12:3x 的第一版一樣；取消延遲 17.5 ms（I4 100）。
- `perf_ab` 兩個 back end 各 3 次（準確度與 A/B 比較每次都一樣）：`perf.md` §2 改成兩邊並排，加上 A/B 比較的表；`explainer.html` 的 `ab-data` 換成實測的 6 種情境（低對比、重模糊、40% 反光、雜訊 12、光照不均、乾淨），不再是「示意」。
- **HALCON 三個不接受的差異**（超過 T6a／T6b，依 ADR-004 的界線不是接受的差異；記在 ADR-004 §5 的 B1–B3）：B1 低對比時除了樣本一個都沒找到；B2 重模糊時多 812 個候選（551 個重複偵測、其餘多半在半個格距，242 個誤判），use case 130–189 ms、峰值多約 80 MB；B3 雜訊 12＋模糊 2 位置 p95 0.933 px、最大 1.360 px。
- ADR-004：狀態、D3、D5、§3 的 HALCON 欄、§5、證據；`spec.md` 的檢查清單；`current-status.md`、`decisions.md`、`AI_CONTEXT.md`（HALCON 第四次讀不到之後 10-12 恢復）、S10 計畫的提醒（6）。順便修掉 `current-status.md` 一個寫壞的路徑（`D:\Benano\20261007_4677_AdvancePin_Proj`，S09 寫入時反斜線被吃掉）。

### 修改的檔案
- docs/adr/004-backend-divergence.md
- docs/steps/S09-contract-ab/perf.md、spec.md、explainer.html
- docs/current-status.md、docs/decisions.md、docs/plan/S10-perf-harness.md、AI_CONTEXT.md
- docs/changelog-ai.md、.ai/handoff.md

### 驗證
- `scripts/dev/test.ps1 -Filter 'tst_halcon_backend'`：17 項，Totals 0 skipped。
- `scripts/dev/test.ps1`（Release＋OpenCV＋HALCON）56／56；`-Config Debug` 56／56。契約測試的 HALCON 列、`tst_halcon_*`、`tst_cli` 都 0 skipped；「有跳過項目的測試」只剩 S09 之前就有的 5 個測試的同一組項目。
- `scripts/dev/test.ps1 -Perf -Filter 'perf_ab'` 3 次（兩個 back end）。`-NoOpenCv`、`-NoHalcon` 的組合沒有重跑（程式沒有改）。`perf_compare_ui` 沒有重跑（不用 HALCON）。
- `explainer.html` 在內建瀏覽器打開：6 個情境都載入，低對比「只有 A」840、重模糊「只有 B」812，和 `perf.md` 一致。
- `node tools/ste-lint.mjs --strict`：899 句，0 error、0 warning。

### 未完成
- B1–B3 要不要修、在哪一步修、要不要把這三種板子加進 `tst_contract_end_to_end` 當成回歸測試：**等使用者決定**（`HalconSearchOptions` 的預設在「不要動」清單）。原因沒有查，ADR-004 寫的是推測。

### 注意事項
- B2 的分類（重複偵測、半個格距）來自 `build/qmake-release/test-work/ab-blur4.json`；`build/` 不入版控，要重看就重跑 `perf_ab`。
- HALCON 的峰值記憶體增量常常接近 0（HALCON 重複使用配置過的記憶體），只能看大的差別。

## 2026-10-09 — Claude Code (Claude Opus 5.5)（S09：契約測試與 A/B 比較）

### 完成
- **契約測試**（`tests/contract/`）：`ContractSupport.h`（資料列 ＝ `app::makeBackendRegistry()` 的每個 back end，每個只建一次；沒編進來或沒有授權的那一列 `QSKIP` 並寫原因；`QCOMPARE_ERROR`）、`ContractTolerances.h`（容差唯一的地方，附依據；`profileOf`：fake 不讀像素、沒有樣本庫比對器，HALCON 強雜訊的角度改用角落位移）、`tst_contract_searcher`（錯誤碼、分數與排序、下限、maxMatches、角度範圍、區域、空結果、取消在 I4 內、可重入、座標原點、子影像、角度正負號、角度掃描、三種格式）、`tst_contract_end_to_end`（use case × 8 種板子 × 會讀像素的 back end）、`tst_contract_library_matcher`（辨識、排序、位姿、N、區域、空的、錯誤、兩邊一致）、效能量測 `perf_ab`（12 種 5 MP 情境並排；只有一個 back end 時 A/B 那一列 SKIPPED）。
- 草稿合併：`tst_opencv_end_to_end`、`tst_halcon_end_to_end`、`tst_opencv_library_matcher` 刪除；`tst_opencv_backend`、`tst_halcon_backend`、`tst_halcon_library_matcher`、`tst_fake_backend` 只留各自才有的事。
- **契約找到的 Liskov 差異**：假 back end 不檢查角度範圍（NaN、顛倒時「成功、找不到」）→ 改成和 OpenCV、HALCON 一樣回 `InvalidArgument`。port 標頭加指向契約測試的註解與「參數不合法回 InvalidArgument」一行（語意沒有改）。
- **A/B 比較**：application `compare/BackendComparison`（配對、分類、差異的原因、`runSideBySide`）；presentation `compare/`（`ComparePresenter` 自己的 JobRunner、背景建立 back end、門檻兩邊各一個；`CompareWindow` ＋ `CompareWindow.ui`；`CompareOverlayItem` 標記畫成小圖）；`MainWindow` 的〔A/B 比較…〕（`ICompareEntryView`）、`AutoRoiPresenter` 的 `ICompareSource`；`copy.json` 新增 46 個 `ui.compare.*`。
- 先量再改（A/B 疊圖）：表格的 `ResizeToContents` 改成填完再算一次欄寬（填表 662 → 4 ms）、標記改成小圖（5 600 個標記的疊圖 96 → 4 ms）、欄寬只看前 64 列。
- 文件：ADR-004（新增；`docs/decisions.md`）、`docs/steps/S09-contract-ab/` 的 `spec.md`、`diagram.md`、`explainer.html`（資料在 HALCON 恢復前是標明「示意」的）、`storyboard.md`、`perf.md`；`architecture.md`、`AI_CONTEXT.md`、`current-status.md`、`docs/plan/S10`–`S12` 的提醒。

### 修改的檔案
- 新增：`src/application/compare/BackendComparison.*`、`src/presentation/compare/*`（含 `.ui`）、`tests/contract/*`、`tests/core/tst_backend_comparison.cpp`、`tests/ui/tst_compare.cpp`、`tests/ui/perf_compare_ui.cpp`、對應的 `AiInspTool/tests/**/*.pro` 與 `contract_tests.pri`、`docs/adr/004-backend-divergence.md`、`docs/steps/S09-contract-ab/*`
- 修改：`src/infrastructure/backend_fake/FakeBackend.cpp`、`src/application/ports/{IPatternSearcher,ITemplateBuilder,ILibraryMatcher}.h`（註解）、`src/presentation/shell/MainWindow.{h,cpp,ui}`、`src/presentation/presenters/{AutoRoiPresenter,AppPresenters}.{h,cpp}`、`docs/ux/copy.json`、`AiInspTool/src/{application,presentation}/*.pro`、`AiInspTool/tests/tests.pro`、`tests/backend/tst_{opencv,halcon}_backend.cpp`、`tests/backend/tst_halcon_library_matcher.cpp`、`tests/core/tst_fake_backend.cpp`、文件（見上）
- 刪除：`tests/backend/tst_{opencv,halcon}_end_to_end.cpp`、`tests/backend/tst_opencv_library_matcher.cpp` 與它們的 `.pro`

### 驗證
- `scripts/dev/test.ps1`（預設，最後一版）：56／56 通過；HALCON 讀不到 dongle（錯誤 2300），`tst_contract_*` 的 HALCON 列、`tst_halcon_*`、`tst_cli` 的 HALCON 部分是 SKIPPED。
- `-Config Debug`（最後一版）：56／56 通過（HALCON 同上 SKIPPED）。`-NoHalcon` 54／54、`-NoOpenCv` 53／53、兩個都關 51／51（這三個在 UI 的最後兩次修改之前跑；那兩次修改和選項無關。兩個都關的組合抓到 `makeMatcher` 沒用到的參數 `/WX`，修好後重跑通過）。
- HALCON 能用時（12:3x，契約測試第一版）：`tst_contract_searcher` 61 passed 0 skipped、`tst_contract_end_to_end` 22／0、`tst_contract_library_matcher` 21／0；數字在 `perf.md` §1。
- 變異測試（手動，改一處 → 只重建那個程式庫與 `tst_contract_searcher` → 跑 → 改回）：4 個都在對的那一列失敗（`spec.md`）。
- `test.ps1 -Perf -Filter 'perf_compare_ui'` 連跑 3 次：AB-U1a p95 19.6–27.0 ms（≤ 33）、AB-U1b 30.5–38.8 ms（≤ 50），都通過；`perf_ab` 3 次（只有 OpenCV）。
- `node tools/ste-lint.mjs --strict`：899 句 0 error 0 warning；`node tools/check-deps.mjs`：0 violation；說明頁在瀏覽器裡實際操作過（拖門檻、點差異），沒有 console 錯誤。
- **未驗證（使用者 2026-10-09 決定之後再測）**：最後一版在 HALCON 上的所有測試、`perf_ab` 的 HALCON 那一邊與 A/B 比較。

### 未完成
- HALCON 恢復後：`perf.md` §6 的步驟（`tst_halcon_backend` 0 skipped → 預設與 Debug → `perf_ab` 3 次 → 補 `perf.md` §2、ADR-004 §3、`explainer.html` 的 `ab-data`）。
- 等使用者決定（ADR-004）：分數門檻要不要依 back end 分開、`nativeGray16` 要不要當預設、新的真實影像能不能用。

### 注意事項
- 容差只改 `ContractTolerances.h`，放寬要在 ADR-004 寫理由；新的 back end 在組裝根註冊就自動多一列。
- 在成功的 `Result` 上呼叫 `error()` 會丟例外、整個測試執行檔中止：契約測試用 `QCOMPARE_ERROR`。
- `test.ps1` 以 `<建置>/test-work` 為工作資料夾：測試的輸出檔寫在目前的資料夾（不要再加 `test-work/`）。
- 表格不要用 `ResizeToContents` 填大量資料（每個 `setItem` 都重量整欄）。

## 2026-10-09 — Claude Code (Claude Opus 5.5)（修掉 session B 的四個已知問題）

### 完成
- 使用者要求把 session B 回報的已知問題都修掉（`e40dfca`，分支 `fix/backend-followups`）。設計寫在 ADR-021 的「後續（2026-10-09）」與 ADR-017 的後續。
- **HALCON 授權失敗之後不必重新啟動**：`halconLicense()` 改成 `HalconLicenseCache`（能用才記住，失敗的下一次先 `is_license_valid` 再探測）；組裝根在這個行程碰 HALCON 之前先用另一個行程檢查（`AiInspTool.exe --probe-halcon-license`，`app::createHalconBackendSafely`），因為實測 HALCON 自己會記住第一次的失敗（錯誤 2036）。切換失敗的卡片多〔再試一次〕。
- **`SESSIONS=1`**：驗證三個行程可以同時拿到授權（新測試）。
- **錯誤卡片**（搜尋時 back end 無法使用）：標題改成 back end 名稱、原因放在做法清單、加〔改用 …〕；建議的是可用的、最完整的另一個（HALCON → OpenCV）；換好之後用新的 back end 再試一次。
- `copy.json` 刪掉 `ui.status.summary`、`ui.status.none`，新增 `ui.backend.switchTo`。

### 修改的檔案
- 程式：`src/app/CompositionRoot.*`、`main.cpp`、`src/infrastructure/backend_halcon/HalconLicense.*`、`src/application/backend/BackendSwitch.*`、presentation 的 `AppPresenters.cpp`、`AutoRoiPresenter.*`、`BackendPresenter.*`、`ControllerInterfaces.h`、`MessageTexts.*`；`docs/ux/copy.json`
- 測試：`tests/backend/tst_halcon_backend.cpp`、`tests/core/tst_backend_registry.cpp`、`tst_backend_switch.cpp`、`tst_cli.cpp`、`tests/ui/PresenterRig.h`、`tst_messages.cpp`、`tst_presenter.cpp`
- 文件：ADR-021、ADR-017、`docs/decisions.md`、`ux-spec.md`、`architecture.md`、`AI_CONTEXT.md`、`current-status.md`、`plan/S09-contract-ab.md`、`.ai/handoff.md`、本檔

### 驗證
- HALCON 的實驗（scratchpad 的小程式，不在 repo）：早上 HALCON 讀不到 dongle（錯誤 2300，Windows 看得到 `VID_064F` 序號 7614034，沒有別的行程佔著）；兩個行程每 2 秒重試 15 分鐘都是 2300，每次呼叫 100–150 ms。使用者當時不能動 dongle；10:28 之前自己恢復，原因不明。恢復後：`HALCONROOT` 指到空資料夾 → 2036，同一個行程改回 `HALCONROOT`、`is_license_valid` 都還是 2036（每次 0 ms）→ HALCON 會記住第一次的失敗。另一個行程檢查授權約 1.1 秒（`AiInspTool.exe` 啟動約 0.17 秒）。
- `scripts/dev/test.ps1`（Release＋OpenCV＋HALCON）54／54：`tst_halcon_backend` 32 項（多 3 個快取的規則）、`tst_halcon_end_to_end` 12、`tst_halcon_library_matcher` 9，0 跳過；`tst_cli` 17 項 0 跳過（`aHalconFailureDoesNotStickOnceTheLicenseIsBack`：授權不在時這個行程不碰 HALCON，授權回來就能用；`severalProgramsCanUseHalconAtTheSameTime`；`theLicenseProbeOptionAnswersWithoutAWindow`）；`tst_backend_registry` 3 項新規則；`tst_presenter` 新增 4 項。真的 HALCON 背景切換：總共 2 755 ms（之前 1 289–1 352），`selectBackend` 0 ms 返回，UI 事件迴圈最長停頓 12 ms。
- 其他組態（同一個 commit 的程式）：`-NoHalcon` 51／51、`-NoOpenCv -NoHalcon` 46／46、`-Config Debug` 54／54（HALCON 0 跳過；真的 HALCON 切換 2 855 ms、UI 最長停頓 12 ms）。
- `ste-lint` 0 errors、`check-contrast` 0 failures、`check-ai-structure` OK。

### 未完成
- dongle 讀不到（錯誤 2300）之後，同一個行程能不能自己恢復沒有驗證（要拔插 dongle）；現在的做法不依賴它（碰 HALCON 之前先用另一個行程檢查）。兩次檢查之間授權剛好掉了的罕見情況，仍然要重新啟動（訊息會說）。
- 早上錯誤 2300 的原因不明（第三次：10-07 中午、10-07 15:06 到 10-08 13:20、10-09 早上）。

### 注意事項
- 第一次用 HALCON 多約 1.2 秒（另一個行程的檢查）；`--backend halcon` 啟動也是。S09 的 A/B 量測不要把它算進搜尋的時間（S09 計畫開頭的提醒）。
- `aHalconFailureDoesNotStickOnceTheLicenseIsBack` 要在 `tst_cli` 行程碰 HALCON 之前跑（它是 HALCON 那一段的第一個；行程已經碰過時會跳過並說原因）。
- 錯誤卡片建議改用的是「可用、不是目前的最後一個」：`makeBackendRegistry` 的註冊順序由簡單到完整（fake、opencv、halcon），新增 back end 時照這個順序登記。

## 2026-10-08 — Claude Code (Claude Opus 5.5)（S09 之前的 session B：授權提醒的卡片、執行中切換 back end、Designer 的 .ui）

### 完成
- 第 1 件（`60197d3`）：HALCON 授權的提醒改成訊息列的**提醒卡片**（按〔關閉〕才收起；被預覽、無匹配、錯誤的卡片或進度列蓋過之後再出現）＋**狀態列最左邊的 back end 標示**（「HALCON · runtime 授權」，滑鼠移上去看完整說明）；4 秒的吐司拿掉，stderr 那一行保留。提醒跟著 back end 交出去（`Backend::notices`），`app::backendNotices()` 移除；新的 `BackendPresenter`、`IBackendView`；摘要不再重複 back end 名稱。
- 第 2 件（`65109a2`）：back end 選單可以在**執行中切換**（ADR-021）。`SearchEngine`（back end ＋ use case，`shared_ptr` 共用）、`BackendSwitch`（規則，沒有 Qt）、`planStartupBackend`；背景執行緒建立；搜尋中先作廢、換好之後依狀態重搜同一個區域；失敗時回到原本的並在訊息列顯示 `ui.error.backend.*`；**啟動時用上次用的**（`QSettings` 的 INI）。`CoreServices` 的 `backend`、`generateObjects` 合成 `engine`。
- 第 3 件（`c2f6be0`）：主視窗外框、訊息列、空白卡片、物件面板、參數面板的版面改用 **Qt Designer 的 `.ui`**（ADR-022），畫布與兩個面板、訊息列是提升元件。
- 開工時 `main` 已前進到 `75c6b3a`（模型路由，ADR-019、020），分支從那裡開；本 session 的 ADR 因此是 **021、022**（交接 prompt 寫的「下一個是 019」已過期）。

### 修改的檔案
- 新增：`src/application/backend/SearchEngine.*`、`BackendSwitch.*`、`src/presentation/presenters/BackendPresenter.*`、`src/presentation/shell/MainWindow.ui`、`MessageBar.ui`、`EmptyCard.ui`、`src/presentation/objects/ObjectsPanel.ui`、`src/presentation/params/ParamsPanel.ui`、`tests/core/tst_backend_switch.cpp`、`AiInspTool/tests/core/tst_backend_switch.pro`、`docs/adr/021-runtime-backend-switching.md`、`docs/adr/022-designer-forms.md`
- 修改：`src/app/CompositionRoot.*`、`main.cpp`、`src/application/backend/BackendRegistry.h`、presentation 的 `AppPresenters`、`AutoRoiPresenter`、`ControllerInterfaces.h`、`ViewInterfaces.h`、`MainWindow`、`MessageBar`、`EmptyCard`、`ObjectsPanel`、`ParamsPanel`、`ImageCanvas`；`AiInspTool/src/application/application.pro`、`presentation/presentation.pro`、`tests/tests.pro`；測試 `tst_shell`、`tst_presenter`、`PresenterRig.h`、`tst_cli`、`tst_backend_registry`、`tests/backend/` 的 6 個（`engine` 的改名）；`docs/ux/copy.json`（新增 7 個 key）、`ux-spec.md`、`architecture.md`、`decisions.md`、`current-status.md`、`plan/S09-contract-ab.md`、ADR-017、`AI_CONTEXT.md`、`.ai/handoff.md`

### 驗證
- 每個 commit 都跑 `scripts/dev/test.ps1`（Release＋OpenCV＋HALCON）：第 1 件 53／53、第 2 件 54／54、第 3 件 54／54；HALCON 的 3 個測試都是 29／12／9 項、0 跳過；其餘跳過是原本就有的（截圖、EXR、全部編進程式）。
- 第 2 件（`65109a2`，另一個 worktree 實際建置）：`-NoHalcon` 51／51、`-NoOpenCv` 49／49、`-NoOpenCv -NoHalcon` 46／46、`-Config Debug` 54／54（HALCON 0 跳過）。
- 第 3 件（`c2f6be0`）：（另一個 worktree 實際建置）`-Config Debug` 54／54、`-Config Profile` 54／54（Qt Creator 的第三種組態）、`-NoOpenCv -NoHalcon` 46／46；HALCON 0 跳過。五個表單在每一種組態都由 uic 編譯，`/W4 /WX` 沒有警告。
- 真的 HALCON：背景切換總共 1 289／1 342 ms，`selectBackend` 0 ms 返回，UI 事件迴圈最長停頓 12 ms（`tst_presenter`，Release；Debug 也通過 200 ms 的門檻）。真的程式記住 halcon 啟動：stderr 依序是開始換、換成功、授權提醒。
- UI 效能（`test.ps1 -Perf -Filter 'perf_ui*'`，前後交替 4 輪，表在 ADR-022）：I1、U3 沒有變差；U1 有一輪兩邊都受機器負載干擾，補量的一輪改之後較好；U3 照舊超過預算（已知）。
- `ste-lint` 0 errors、`check-contrast` 0 failures、`check-deps` 0 violations、`check-ai-structure` OK。
- Qt Creator 的 Designer：使用者 2026-10-09 在 Qt Creator 用 Designer 打開五個表單：都打得開、版面正常（沒有存檔）。

### 未完成
- 搜尋時 back end 無法使用的錯誤卡片（S08 的 `errorCard`）標題沿用把錯誤訊息當名稱的寫法，沒有改。
- HALCON 的授權探測整個程式只做一次（插上 dongle 後要重新啟動）；`SESSIONS=1` 下從 HALCON 換走會不會佔住授權沒有驗證。

### 注意事項
- 換 back end 的規則在 `application/backend/BackendSwitch`，presentation 只透過 `IBackendController`、`IBackendView`、`ISearchEngineSink`；別在 Presenter 裡加規則。
- 表單的字串一律 `notr`，文案由各類別的 `retranslate` 從 `copy.json` 設定；Designer 新增元件要給 objectName。
- UI 的效能數字受機器負載影響很大，比較時要前後交替量。

## 2026-10-08 — Claude Code (Claude Opus 5.5)（模型與推理強度路由，ADR-020）

### 完成
- 依使用者的第二份設定 prompt，把模型路由升級成同時決定 model 與 effort（[ADR-020](adr/020-adaptive-model-effort-routing.md)，部分取代 ADR-019）：複雜度、風險、推理深度、不確定性 1–5 分加上判準，範圍與驗證成本；五個 subagent（新增 `developer-deep` sonnet/high、`architect-deep` opus/xhigh，原本三個加上 `effort`）；兩次失敗升一級、解決後降級、最高一級仍失敗就停下來問使用者。
- hook 改寫：回傳 `recommended_agent`／`model`／`effort`、四個分數、`scope`、`verification_cost`、`routing_reason`、失敗與升級的次數、主 session 目前的 effort；狀態存在 `%TEMP%\claude-model-router\`；agent 與背景工作的通知不處理（實測它們也會觸發 `UserPromptSubmit`）。
- 新增 `scripts/ai/bench-model-routing.mjs`（`verify`：從 session 紀錄讀實際的 model 與 effort；`bench`：同一組任務在五個 agent 上比較），沒有 `--yes` 不花額度。
- 查證官方文件（`claude-code-guide`）並實測：frontmatter 的 `effort` 會套用；`claude --agent` 不套用 effort；Haiku 4.5 不支援 effort（紀錄沒有 `effort` 欄位）；`haiku` 別名 2.1.292 是 Haiku 4.5、2.1.293 起是 Haiku 5.5（CLI 在 session 期間自動從 2.1.292 更新到 2.1.294）。更正 ADR-019 與第一版文件的「`haiku` 對應 Haiku 4.5」。
- 使用者同意後在分支 `chore/model-effort-routing` commit，以 `--no-ff` 合併到 `main`（沒有 tag、沒有 push）。

### 修改的檔案
- 新增：`.claude/agents/developer-deep.md`、`.claude/agents/architect-deep.md`、`scripts/ai/bench-model-routing.mjs`、`docs/adr/020-adaptive-model-effort-routing.md`
- 修改：`.claude/hooks/model_router.mjs`、`.claude/agents/fast-worker.md`、`developer.md`、`architect.md`、`scripts/ai/check-model-routing.mjs`、`MODEL_ROUTING.md`、`CLAUDE.md`、`AI_CONTEXT.md`、`docs/adr/019-model-routing.md`、`docs/decisions.md`、`docs/changelog-ai.md`、`docs/current-status.md`、`.ai/handoff.md`

### 驗證
- `node scripts/ai/check-model-routing.mjs`：68 項通過（第一次跑有一項沒過：已在最高一級時，短的失敗回覆不輸出「停下來」的提示；已修正）。
- `bench-model-routing.mjs verify --yes`（Claude Code 2.1.294）：五個 subagent 實際的 model 與 effort 都照設定（haiku-5-5/low、sonnet-5-5/medium、sonnet-5-5/high、opus-5-5/high、opus-5-5/xhigh），US$0.86。
- `bench-model-routing.mjs bench --yes`：3 題 × 5 個 agent，15 次全對、effort 都照設定；費用合計 US$3.73，幾乎都是啟動時的快取寫入。題目太簡單，正確率分不出高下（`MODEL_ROUTING.md` 第 9 節）。
- 費用都是 Claude Code 回報的 `total_cost_usd`；這次升級共約 US$4.7。
- 互動式（桌面 app）：改版前 subagent 的回報會觸發 hook、被當成長需求打分；改版後開一個背景 agent 測試，它的回報沒有再帶出路由提示。使用者下一則訊息的 context 是新的 JSON 格式，但沒有 `main_session_effort`：`UserPromptSubmit` 的 hook 拿不到 `CLAUDE_EFFORT`（同一個 session 的 Bash 工具拿得到 `max`），文件已改成「拿不到就沒有」。
- `check-ai-structure.ps1`、`ste-lint`（0 errors）、`git diff --check`：通過。沒有改產品程式碼，沒有建置、沒有跑 `test.ps1`。

### 未完成
- 要比出正確率的差異，需要更難、答案能自動判斷的任務（每次多花數美元），沒有做。
- 沒有 Git Bash 的電腦（hook 改由 PowerShell 執行）沒有驗證。
- 這個 session 開始時沒有 `.claude/agents/`，五個 subagent 要新的 session 才看得到（官方文件；`verify` 用 headless 的新 session 驗證過）。

### 注意事項
- 使用者的 prompt 寫 `model_router.py`；沿用 ADR-019 的 Node（`python.exe` 是 Store 捷徑）。
- 環境變數 `CLAUDE_CODE_EFFORT_LEVEL` 會蓋過所有 subagent 的 effort；檢查腳本在它有設定時警告。
- 2.1.292 以前的 Claude Code 上 `fast-worker` 的 effort 不會生效（Haiku 4.5 不支援），`verify` 會報降級。

## 2026-10-08 — Claude Code (Claude Sonnet 5.5，中途換成 Claude Opus 5.5)（模型路由）

### 完成
- 依使用者提供的 ChatGPT 討論與其中的設定 prompt，建立模型路由（[ADR-019](adr/019-model-routing.md)）：根目錄的 `MODEL_ROUTING.md`（`CLAUDE.md` 匯入）；`.claude/agents/` 的 `fast-worker`（haiku，唯讀）、`developer`（sonnet，不能再委派）、`architect`（opus，唯讀，輸出修改計畫）；`UserPromptSubmit` hook `.claude/hooks/model_router.mjs`（Node，關鍵字分類，只注入提示）；驗證腳本 `scripts/ai/check-model-routing.mjs`。
- 第一版只寫規則（`.claude/model-routing.md`，沒有 hook 與 subagent）；使用者接著貼上完整的設定 prompt，規則搬到根目錄並加上 subagent 與 hook。session 中途主模型由 Sonnet 5.5 換成 Opus 5.5。
- 使用者同意後在分支 `chore/model-routing` commit（`1c4f264`），再以 `--no-ff` 合併到 `main`（沒有 tag、沒有 push）。

### 修改的檔案
- 新增：`MODEL_ROUTING.md`、`.claude/agents/fast-worker.md`、`.claude/agents/developer.md`、`.claude/agents/architect.md`、`.claude/hooks/model_router.mjs`、`scripts/ai/check-model-routing.mjs`、`docs/adr/019-model-routing.md`
- 修改：`.claude/settings.json`（只加 `hooks`，`permissions` 沒動）、`CLAUDE.md`、`AI_CONTEXT.md`、`docs/decisions.md`、`docs/adr/000-ai-agent-structure.md`、`docs/changelog-ai.md`、`docs/current-status.md`、`.ai/handoff.md`

### 驗證
- `node scripts/ai/check-model-routing.mjs`：44 項通過（含用 Git Bash 跑 `settings.json` 的實際指令字串）。
- 檢查腳本本身：在 scratchpad 的複本故意改壞 9 處（model 寫版本號、唯讀的加 Edit、拿掉 `disallowedTools`、刪一條權限、`bypassPermissions`、`PowerShell(*)`、hook 的 Opus 分支失效、`CLAUDE.md` 不匯入），每一處都報 FAIL。
- headless 實測 `claude -p … --model haiku --output-format stream-json --verbose`（Claude Code 2.1.292）：hook 有執行（context 的提示與 hook 的輸出相同）；`system/init` 列出三個 subagent；三個同時委派、回報回到主 session；實際模型 `fast-worker` → `claude-haiku-4-5-20251001`、`developer` → `claude-sonnet-5-5`、`architect` → `claude-opus-5-5`。這次測試 US$0.59（API 計價）。
- 互動式 session（桌面 app 的 Code 分頁）：改完 `settings.json` 之後，這個 session 的下一則使用者訊息的 context 就有「[模型路由提示]」，不必重新開 session。
- `check-ai-structure.ps1`、`node tools/ste-lint.mjs`（0 errors）、`git diff --check`：通過。
- 沒有建置、沒有跑 `scripts/dev/test.ps1`（沒有改產品程式碼）。

### 未完成
- 沒有 Git Bash 的電腦（Claude Code 改用 PowerShell 執行 hook）沒有驗證。

### 注意事項
- **`haiku` 別名在 Claude Code 2.1.292 對應 Haiku 4.5，不是 5.5**；照使用者的要求仍用別名。
- 每個 subagent 啟動時載入 `CLAUDE.md` 與匯入的文件（48K–68K tokens）：委派有固定成本（Sonnet 約 US$0.17、Opus 約 US$0.24 起跳）。
- hook 只注入提示，不切換主 session 的模型；`AOI_MODEL_ROUTER=off` 可以關掉。
- ChatGPT 分享頁的程式碼區塊抓不到；hook 與 subagent 依使用者貼上的 prompt 與官方文件（經 `claude-code-guide` 查證）寫，不是照抄來源的範例。來源範例的 Python 在這台機器不能用（`python.exe` 是 Store 捷徑），所以改 Node。

## 2026-10-08 — Claude Code (Claude Opus 5.5)（session A 的後續：HALCON 授權恢復後補驗證）

### 完成
- 2026-10-07 15:06 起 HALCON 拿不到授權（錯誤 2300）。2026-10-08 依序試過：重插、換 USB 埠（原本經過 Genesys Logic 的 Hub）、插電腦本體、真正重新啟動（開機時間 12:47:40），都沒恢復；授權檔、runtime、環境變數、系統更新都沒有變化。使用者在另一台電腦跑 `hhostid` 也看不到這個 dongle，而且使用者自己的專案（`D:\AOI_Project\SType\AlgoParamFineTune\BnSDKLibsController\CV_Algorithm\Algo\Algo_SkewInspect`，呼叫 HALCON 的 API）插著 dongle 時能檢測、拔掉時報 2041 Invalid host。13:20 起我們的程式又拿得到授權，原因不明。
- **更正**：之前說「`hhostid` 看不到 dongle 就代表 HALCON 讀不到」是錯的——`hhostid`（24.11 與 22.11）在這台與另一台都不列出這個 dongle。之前說「沒有別的行程佔著 dongle」也不完整：只用名稱找了本專案的程式與 HDevelop（13:20 時改用「載入 HALCON DLL 的行程」查，沒有）。
- 補跑 HALCON 的驗證並更新 ADR-018 的「驗證」、`current-status.md`、`.ai/handoff.md`、`AI_CONTEXT.md` 的 HALCON 那一列。

### 修改的檔案
- `docs/adr/018-qmake-only-build.md`、`docs/current-status.md`、`.ai/handoff.md`、`AI_CONTEXT.md`、`docs/changelog-ai.md`

### 驗證
- `test.ps1 -NoBuild -Config Debug`：53／53，`tst_halcon_backend` 29、`tst_halcon_end_to_end` 12、`tst_halcon_library_matcher` 9 項通過、0 跳過；`tst_cli` 通過。
- `test.ps1 -NoBuild -Config Profile -BuildDir AiInspTool\build\Desktop_Qt_6_12_0_MSVC2022_64bit_Profile`：53／53，HALCON 同上。
- `test.ps1 -NoBuild -Perf -Filter 'perf_halcon'`（Release）：通過；T1 p95 5.3 ms、T2b 22.5、T4 26.5、T6a 100%、T6b 0.485 px、授權第一次檢查 1.45 s、取消延遲 p95 19.2 ms（S08：7.0–22.0、31.5–82.2、28.6–46.6、100%、0.48、1.55–1.87、19–30）。預算沒有改。
- Qt Creator 模擬的 Release：`AiInspTool.exe --backend halcon <0_0_B.tif>` 開圖成功，訊息列與 stderr 都有授權提醒（截圖確認）。

### 未完成
- 錯誤 2300 的原因不明。授權 `SESSIONS=1`：別的 HALCON 程式同時在跑會不會讓我們拿不到授權，沒有驗證。

### 注意事項
- 判斷 HALCON 能不能用：`test.ps1 -Filter 'tst_halcon_backend'`，看 log 的 Totals 是不是 0 skipped；不要用 `hhostid`。

## 2026-10-08 — Claude Code (Claude Opus 5.5)（S09 之前的 session A：建置改成只用 qmake）

### 完成
- 使用者 2026-10-07 的決定：建置只用 qmake（`AiInspTool/AiInspTool.pro` 位置不變，包含所有程式庫、程式與所有 Qt Test）、移除 CMake、執行檔改叫 `AiInspTool.exe`、OpenCV 與 HALCON 預設編進去、Qt Creator 的 Debug／Release／Profile 都要能建置。session 中使用者決定：精靈產生的 `AiInspTool/main.cpp`、`mainwindow.*` **保留**；主目錄 `AiInspTool/` 的舊原地建置輸出與 `.qtcreator/AiInspTool.pro.user` 由 agent 在合併前刪除。分支 `build/qmake-only`（從 `main` 的 `bd4e285`），在 worktree 做。
- 先用小範例驗證推論：subdirs 的子專案寫成 `../lib/lib.pro` 時，輸出跑到 `<建置>/../lib`（建置資料夾外面，三種組態共用）→ 所有 `.pro`／`.pri` 放在 `AiInspTool/` 底下對照 `src/`、`tests/`，原始碼用 `$$AOI_ROOT` 引用（ADR-018）。
- qmake 專案：`AiInspTool.pro`（subdirs；qmake 時印出這次建置有哪些功能）、`.qmake.conf`、`qmake/`（`aoi_options`、`aoi_deps`＝依賴表、`aoi_opencv`、`aoi_halcon`、`aoi_target`、`aoi_lib`、`aoi_test`）、`src/src.pro`＋13 個程式庫＋`app/app.pro`（`AiInspTool.exe`，GUI 子系統）、`tests/tests.pro`（測試清單，qmake 時寫 `aoi-tests.txt`、`aoi-perf-tests.txt`）＋`test_support.pro`＋59 個測試的 `.pro`＋5 個資料夾的 `*_tests.pri`。檔案明確列出。`aoi_ux.qrc` 搬到 `AiInspTool/src/presentation/`，由用到 presentation 的程式內嵌；OpenCV 的 DLL 由 `backend_opencv.pro` 的 `COPIES` 複製到 `<建置>/bin`；`/IGNORE:4199`（LNK4199，連了組裝根但沒用到 HALCON 的測試，29 個）。
- `tests/support/test_environment.cpp`：每個測試在 `main` 之前設 offscreen 與字型（不覆寫已有的設定），Qt Creator 直接執行測試也一樣。
- `scripts/dev/test.ps1` 改寫：qmake → `jom qmake_all` → jom（和 Qt Creator 相同）；`-Config Release|Debug|Profile`、`-NoOpenCv`、`-NoHalcon`、`-Perf`、`-Filter`、`-NoBuild`、`-BuildDir`、`-TimeoutSec`、`-Jobs`；照清單跑、每項逾時、失敗印 log、列出通過數與「有跳過項目的測試」、4 個 node 檢查（`check_deps_catches_a_violation` 要失敗才算通過）。`env.ps1` 改成 MSVC＋qmake＋jom。兩個腳本都是 UTF-8 with BOM（PowerShell 5.1）。
- `CompositionRoot.cpp` 的「沒有編進程式」訊息改成 qmake 的說法（保留 `AOI_WITH_OPENCV=ON`／`AOI_WITH_HALCON=ON` 字樣）；註解裡的 CMake／CTest／`aoi.exe`（`HalconRuntime.h`、`perf_halcon*.cpp`、`perf_opencv_real.cpp`、`tst_cli.cpp`、`tst_lattice_crosscheck.cpp`、`tst_lattice_explainer.cpp`、`tools/check-deps.mjs`）。
- 證明等價之後移除 CMake：21 個 `CMakeLists.txt`、`cmake/`（2 個）、`CMakePresets.json`。`.gitignore`（根目錄與 `AiInspTool/`）改成 qmake 的輸出與 Qt Creator 的快取。
- 文件：ADR-018（新）、ADR-003 與 ADR-017 的「後續」、`decisions.md`、`AI_CONTEXT.md`（技術棧、環境、目錄、建置與測試、VERIFIED、Context Routing）、`docs/architecture.md`（依賴改由 `aoi_deps.pri` 表達、模組表）、`README.md`、`docs/plan/S09`–`S12`（改掉 preset／label／CTest 的說法，加「建置改 qmake 之後的提醒」）、`docs/perf-test-plan.md`、`docs/00-master-plan.md`（一格）、`docs/current-status.md`。`docs/steps/S02`–`S08` 是歷史紀錄，不改（對照表在 ADR-018）。

### 修改的檔案
- 新增：`AiInspTool/.qmake.conf`、`AiInspTool/qmake/*.pri`（7 個）、`AiInspTool/src/**.pro`（15 個）、`AiInspTool/tests/**`（`tests.pro`、`support/test_support.pro`、59 個測試 `.pro`、5 個 `*_tests.pri`）、`tests/support/test_environment.cpp`、`docs/adr/018-qmake-only-build.md`
- 搬移：`AiInspTool/aoi_ux.qrc` → `AiInspTool/src/presentation/aoi_ux.qrc`
- 修改：`AiInspTool/AiInspTool.pro`、`AiInspTool/.gitignore`、`.gitignore`、`scripts/dev/env.ps1`、`scripts/dev/test.ps1`、`src/app/CompositionRoot.cpp`、上面列的註解、文件
- 刪除：`CMakeLists.txt`、`CMakePresets.json`、`cmake/AoiTargets.cmake`、`cmake/FindHalconLocal.cmake`、`src/**/CMakeLists.txt`（13 個）、`tests/**/CMakeLists.txt`（7 個）

### 驗證
- 8 種組合（`test.ps1` 的 Release／Debug × 兩個都關／`-NoHalcon`／`-NoOpenCv`／預設）移除 CMake 前後各一輪，全部通過：45／50／48／53（Release 與 Debug 相同，和 CTest 時代的數量一樣）。第一輪 HALCON 的測試在 Release、Debug 都有跑（29／12／9 項，0 跳過）；第二輪 Release 也有跑，**第二輪 Debug 的 HALCON 測試因為讀不到 dongle（錯誤 2300）整組跳過**。
- Qt Creator 的三種組態（Qt Creator 20 的 qmake 參數、建置資料夾 `AiInspTool/build/Desktop_Qt_6_12_0_MSVC2022_64bit_{Debug,Release,Profile}`）：建置成功、0 個警告、各 60 個執行檔與 OpenCV 的 DLL；旗標看過（Release／Profile 有 `-O2 -DNDEBUG`，Debug `-MDd` 沒有 `NDEBUG`）。Profile 跑全部測試：53／53（HALCON 的 3 個跳過，dongle）。
- 實際啟動：Release 開使用者的 `0_0_B.tif`（16 bit LZW）成功（截圖確認影像、偵測 12 bit），`--backend halcon` 時 stderr 有授權提醒（2026-10-07，dongle 還讀得到時）；Qt Creator 模擬的 Release：`--backend opencv` 開同一張 TIFF 成功，`--backend halcon` 在讀不到 dongle 時結束碼 2、原因寫在 stderr。`--help`、參數錯誤經管線執行時照常寫到 stdout／stderr。
- Qt Test 的純文字輸出不寫到被接走的 stdout，`-xml`、`-lightxml` 會寫（實測）。
- qmake 的錯誤：`AOI_OPENCV_DIR`、`AOI_HALCON_SDK_DIR` 指錯、`AOI_WITH_HALCON=no` 都以結束碼 3 停下並說明。
- `test.ps1 -Perf`（Release，單次）：10 項通過；T1 0.31、T2a 56.8、T4 29.0 ms、T6b 0.106 px、T3 0.48、U1b 3.9、U4-PNG 100、I1 6.9、U3 16.5（已知超標）、I3 25.9 ms，和 S05–S07 的文件同一個量級。`perf_halcon` 因為 dongle 跳過。預算沒有改。
- `node tools/check-deps.mjs`（0 違規）、`node tools/ste-lint.mjs`（0 error）、`node tools/check-contrast.mjs`（0 失敗）、`scripts/ai/check-ai-structure.ps1`（OK）。

### 未完成
- **HALCON 讀不到 dongle（錯誤 2300）時的驗證**：重插 dongle 後補跑 `test.ps1 -Config Debug -Filter 'tst_halcon*'`、`test.ps1 -Perf -Filter 'perf_halcon'`。
- 新的專案還沒有在 Qt Creator 的畫面裡打開過（只用命令列模擬同樣的步驟）。

### 注意事項
- 建置進行中不要改 `.pro`／`.pri`：Makefile 會觸發 qmake 重跑。
- `scripts/**/*.ps1` 要存成 UTF-8 with BOM；bash 的 heredoc 會吃掉反斜線，改 `.pri`、JSON 用檔案寫入。
- qmake 把 `#` 當成註解的開頭（引號裡也是）；`$$cat()` 的結果要先放進變數再 `for`。
- 不要把 `/IGNORE:4199` 拿掉（理由在 `aoi_target.pri`）；不要把測試清單改成看 `bin/` 的檔案。

## 2026-10-07 — Claude Code (Claude Opus 5.5)（S08：HALCON back end）

### 完成
- 依 Step Protocol 做 S08，在 worktree 的分支 `step/08-halcon`（從 `main` 的 `000aa36` 開出）。
- 開工時實測授權並向使用者說明：runtime（RTL，`3-7614034`）可用、SDK（`3-7081033`）的 dongle 沒插、`%HALCONROOT%` 沒有開發檔。使用者指示從 CV_Algorithm 專案複製開發檔，並要求分辨兩種 dongle、授權錯了要提醒。
- `third_party/halcon/`（24.11.2 的標頭＋`halconcpp.lib`，`-text`、`.gitignore` 例外、`README.md`）；`cmake/FindHalconLocal.cmake`（`Halcon::HalconCpp`、`/DELAYLOAD:halconcpp.dll`、`NOMINMAX`）；preset `release-halcon`／`debug-halcon`（OpenCV＋HALCON）；`test.ps1 -Preset release-halcon`。
- HALCON back end（`src/infrastructure/backend_halcon/`，ADR-017）：範本建立器（`create_shape_model`）、搜尋器（`find_shape_model`，視窗清單 ＝ 一個聯集的區域）、樣本庫比對器（一次 `find_shape_models`）、`HalconImages`（緊密排列零複製）、`HalconPose`（位姿換算）、`HalconCancelWatch`（`interrupt_operator`）、`HalconRuntime`（延遲載入的保護、版本）、`HalconLicense`（探測、判斷、提醒）、`HalconErrors`、`HalconSearchSteps`、`HalconBackend`（工廠、`setHalconThreads`）。
- 運算子的行為查 `%HALCONROOT%\help\operators_en_US.ref` 與執行時的 `get_operator_info`（`find_shape_model(s)` 可中斷、'no_restriction'；`create_shape_model` 不可中斷）。
- 實測後改的：'auto' 的角度步進（48×28 給 5.2°，±5° 只剩三個角度）→ 角落每步 0.5 px＋模型範圍兩端多 2 步；位姿要用 `affine_trans_pixel` 的修正（測試證明）；`Gray16` 換 8 bit 由逐像素（213 ms）改查表（8–11 ms）；次像素預設 `least_squares`（試過 `_high`）。
- 組裝根：halcon 的登記（PATH 上有 DLL 才可用、授權在工廠裡）、`backendNotices`；main：提醒寫 stderr（fflush）、吐司、選單提示；`MainWindow::setBackends` 改成 `BackendChoice`（不可用的停用、加 ✕、提示原因）；`copy.json` 新增 2 個 key。
- 測試：`tst_halcon_backend`（29 項）、`tst_halcon_end_to_end`、`tst_halcon_library_matcher`、`perf_halcon`、`perf_halcon_real`；`tst_cli`（有授權啟動並提醒、沒有授權結束碼 2、沒有 DLL 時 fake 照常）、`tst_backend_registry`、`tst_shell`（選單）。
- 文件：`docs/steps/S08-halcon/`（四種輸出＋`perf.md`）、ADR-017、ADR-010／014 的「後續（S08）」、`decisions.md`、`architecture.md`、`AI_CONTEXT.md`、`README.md`、`docs/plan/S09`–`S12` 的「S08 完成後的提醒」；`perf/budgets.json`（T2b calibrated、`searchCalibrationS08`，limit 沒改）、`perf/baseline/casparhsu1-halcon.json`。

### 修改的檔案
- 新增：`src/infrastructure/backend_halcon/`（24 個檔案）、`cmake/FindHalconLocal.cmake`、`third_party/halcon/`、`tests/backend/{tst_halcon_backend,tst_halcon_end_to_end,tst_halcon_library_matcher,perf_halcon,perf_halcon_real}.cpp`、`docs/steps/S08-halcon/`、`docs/adr/017-halcon-shape-model-backend.md`、`perf/baseline/casparhsu1-halcon.json`
- 修改：`CMakeLists.txt`、`CMakePresets.json`、`cmake/AoiTargets.cmake`、`.gitattributes`、`.gitignore`、`scripts/dev/test.ps1`、`src/infrastructure/CMakeLists.txt`、`src/app/{CMakeLists.txt,CompositionRoot.h,CompositionRoot.cpp,main.cpp}`、`src/presentation/shell/MainWindow.{h,cpp}`、`tests/backend/{CMakeLists.txt,BackendTestSupport.h}`、`tests/core/{CMakeLists.txt,tst_backend_registry.cpp,tst_cli.cpp}`、`tests/ui/tst_shell.cpp`、`docs/ux/copy.json`（只新增 key）、`perf/budgets.json`、`AI_CONTEXT.md`、`README.md`、`docs/architecture.md`、`docs/decisions.md`、`docs/current-status.md`、`docs/adr/010-*`、`014-*`、`docs/plan/S09`–`S12`、`.ai/handoff.md`

### 驗證
- CTest：Release 45、Release＋OpenCV 50、**Release＋OpenCV＋HALCON 53**、**Debug＋OpenCV＋HALCON 53**、**只有 HALCON（`-DAOI_WITH_HALCON=ON -DAOI_WITH_OPENCV=OFF`）48** 個項目全過。三個 HALCON 組態是 dongle 恢復後、在最後的程式碼上跑的（HALCON 的測試 0 跳過：29／12／9 項），之後只改了兩處註解，重建並重跑 HALCON、`tst_cli`、`tst_backend_registry`、`tst_shell` 共 6 個項目。Release 與 Release＋OpenCV 是在授權探測與一個 HALCON 測試修改之前跑的：那兩個改動只在 HALCON 的組態編譯。
- 降級（手動與 `tst_cli`）：`HALCONROOT` 指到空資料夾 → `--backend=halcon` 結束碼 2、錯誤 2036；PATH 上沒有 HALCON → halcon 登記成不可用、`--backend=fake` 照常啟動；有授權時 `--backend=halcon` 啟動，stderr 有「SDK 的 dongle 沒插」的提醒。
- `node tools/check-deps.mjs`：0 違規；`--self-test` 15 個 fixture 全過。`node tools/ste-lint.mjs --strict`：0 錯誤、0 警告。`diagram.md` 8 張 Mermaid 用 mermaid 11（jsDelivr，瀏覽器窗格）解析全過，故意寫壞的一張被抓到。`explainer.html`：滑桿、勾選框、主題都操作過，0 個 console 訊息，375 px 寬沒有水平捲動。
- 效能：`-Preset release-halcon -Perf` 連跑 3 次（`AOI_REAL_IMAGE_DIR` 有設）＋`perf_halcon` 補跑 1 次。T1、T4、T6a、T6b 3 次都在預算內；**T2b 3 次中 1 次超過**（p95 82.2 ms，那一次整台機器變慢：S02 的 U4-PNG 也超過、`perf_ui_baseline` 300 秒逾時；第 2、3、4 次 29–35 ms）。U3 仍超標（S04 起）。
- 變異：12 個（角度正負號、沒有旋轉修正、沒有角度餘量、'auto' 步進、不用視窗過濾、拿掉 NMS、提醒規則反過來、Gray16 不計數、沒有取消監看、拿掉 Matching 檢查、視窗不往外擴、角度不夾回範圍），抓到 11 個。第一輪有 3 個因為 `/WX`（變數沒用到）編譯失敗，改成保留變數的寫法重跑；「不用視窗過濾」第一次存活，補了「元件中心在視窗外 1.5 像素」的測試後抓到；「拿掉 domain 的 NMS」存活：同一次 `find_shape_model` 裡 HALCON 自己的 MaxOverlap 已經去掉重複，測過的情境等價，NMS 留著當契約的防禦。
- `AiInspTool.pro`：Qt 6.12.0 的 qmake ＋ jom（Release）建置成功；offscreen 下 `--backend=halcon` 結束碼 2、訊息說要用 `AOI_WITH_HALCON=ON`，`--demo 500` 啟動 4 秒仍在執行。**沒有在 Qt Creator 裡載入過。**
- **真實的「讀不到 dongle」**：驗證途中（12:04）HALCON 突然讀不到**一直插著**的 RTL dongle（錯誤 2300；Windows 仍認得 `USB\VID_064F&PID_2AF9\000007614034`、狀態 OK；`hhostid` 看不到它；沒有行程載入 HALCON）。使用者重插後約 12:25 恢復，**原因不明**。那段期間 `--backend=halcon` 結束碼 2、原因寫清楚，HALCON 的 `tst_*` 跳過並說原因（不是失敗）。因此加了「沒有授權時自己讀 `%HALCONROOT%\license\*.dat`」，訊息寫出每個授權檔需要哪個 dongle。這台電腦另外裝了 HALCON 22.11（使用者的 AppData，授權是過期的試用版）；`HALCONROOT` 指 24.11，所以 22.11 的 HDevelop 讀 24.11 的授權檔，報 2041 Invalid host。

### 未完成
- HALCON 的影像來源（`read_image`）：沒有做，等使用者決定。`create_ncc_model` 的對照：沒有做（plan 寫「可選」）。
- 流程 B 的 UI（使用者決定之後再接）；兩個 back end 的分數門檻要不要分開（S09 一起看）。
- 契約測試的泛化（S09）。

### 注意事項
- **HALCON 的 'auto' 角度步進不能用在小樣本**；`HalconSearchOptions` 的 `cornerShiftPx`、`angleMarginSteps` 是實測定的（`perf.md` 的 AS-*）。
- **位姿換算不是單純 ±0.5**（`HalconPose.h`、ADR-010 的 S08 後續）。
- `halconcpp.dll` 延遲載入：任何 HALCON 呼叫之前要先過 `halconRuntimeProblem()`；組裝根只 include 不帶 HALCON 標頭的三個檔案。
- **PowerShell 的 `Get-Content -Raw`／`Set-Content` 把 UTF-8 檔當 CP950 讀壞過一次**（`HalconSearchOptions.h`，立刻用編輯工具整份重寫）；改檔一律用編輯工具。
- `QVERIFY2` 的訊息在通過時也會求值：`tst_cli` 一開始在訊息裡讀了 stderr，後面就讀不到了。
- 量測前 Visual Studio 自己更新到 17.14.37710（`cl` 19.44.35229）；更新期間 `vswhere` 找不到 VS、`env.ps1` 會失敗。
- 真實影像與疊圖不入版控；`.claude/launch.json`（瀏覽器窗格的暫時伺服器）已刪除。

## 2026-10-06 — Claude Code (Claude Opus 5.5)（S07：OpenCV back end）

### 完成
- 依 Step Protocol 做 S07，在 worktree 的分支 `step/07-opencv`（從 `main` 的 `5cf7748` 開出）。
- 開工時向使用者要真實影像，並說明「有效位元數覆寫」的設計、問了三個做法與流程 B 的範圍：使用者提供兩張去識別化影像（12 bit、24.4 MP，不入版控）；選了「存進板子、換影像沿用」「參數面板的下拉選單」「流程 B 只做 adapter＋測試＋T7，UI 先不接」。12 bit 的存法（16 bit 容器、值 0..4095）使用者在 S06 之後已回答。
- OpenCV back end（`src/infrastructure/backend_opencv/`，ADR-016）：`OpenCvTemplateBuilder`／`OpenCvTemplate`（金字塔 × 旋轉範本，含周圍影像轉）、`OpenCvPatternSearcher`（由粗到細的 NCC、視窗清單、爬坡、拋物線次像素與角度、domain 的 NMS）、`OpenCvCorrelation`（小的分數圖自己算）、`OpenCvPyramid`、`OpenCvSearchOptions`、`OpenCvLibraryMatcher`（`ILibraryMatcher`）、`OpenCvBackend`（工廠、`setOpenCvThreads`）；`OpenCvImages::toGray8`。組裝根在 `AOI_WITH_OPENCV=ON` 時註冊 opencv（`--backend opencv`），`app::workerThreads()` 給 JobRunner 與 OpenCV 共用。
- 第一版 T2a、T4 超過預算（193／87 ms 中位數）：分段計時發現精修的 5×5 分數圖 `matchTemplate` 仍走 DFT；改成整數點積＋積分影像後 64／30 ms，找到的元件完全相同。
- 有效位元數的覆寫：domain `Board::significantBits`、`decideSignificantBits`／`withSignificantBits`；application `InspectionDocument::setSignificantBits`（reset 保留）；storage 配方 schema v2（v1 → v2 遷移補 0；新增範例 `recipe-v2.json`）；presentation 參數面板「影像位元數」、`IParamsController`／`IParamsSink` 各加一個方法、Presenter 開圖與改設定時套用、比推測小時吐司；`copy.json` 新增 5 個 key。
- 測試：`tests/backend/`（`tst_opencv_backend`、`tst_opencv_end_to_end`、`tst_opencv_library_matcher`、`perf_opencv`、`perf_opencv_real`）；`tst_image_view`、`tst_document`、`tst_params`、`tst_presenter`、`tst_recipe_store` 加了位元數與 v2；`tst_backend_registry`、`tst_cli` 依建置選項檢查 opencv。
- 真實影像：HA 的種子分數分兩群（焊墊邊緣 0.70–0.85）、LA 的晶格在空槽補假缺格（S05 已知失敗 #11）——都在 use case／晶格，沒有改，列為使用者決定。
- 文件：`docs/steps/S07-opencv/`（四種輸出＋`perf.md`）、ADR-016、ADR-014／015 的「後續（S07）」、`decisions.md`、`architecture.md`、`AI_CONTEXT.md`、`README.md`、`docs/plan/S08`–`S12` 的「S07 完成後的提醒」；`perf/budgets.json`（T1、T2a、T4、T6a、T6b 標 calibrated，limit 沒改）、`perf/baseline/casparhsu1-opencv.json`。

### 修改的檔案
- 新增：`src/infrastructure/backend_opencv/OpenCv{Backend,Correlation,LibraryMatcher,PatternSearcher,Pyramid,Template,TemplateBuilder}.{h,cpp}`、`OpenCvSearchOptions.h`；`tests/backend/`（6 個檔案）；`tests/infra/fixtures/recipe-v2.json`；`docs/steps/S07-opencv/`；`docs/adr/016-opencv-coarse-to-fine-search.md`；`perf/baseline/casparhsu1-opencv.json`
- 修改：`src/domain/component.h`、`image_depth.{h,cpp}`、`image_view.h`（註解）；`src/application/document/InspectionDocument.{h,cpp}`；`src/infrastructure/backend_opencv/OpenCvImages.{h,cpp}`、`CMakeLists.txt`；`src/infrastructure/storage/JsonCodec.cpp`、`JsonRecipeStore.{h,cpp}`；`src/presentation/params/ParamsPanel.{h,cpp}`、`presenters/{AutoRoiPresenter,ParamsPresenter}.{h,cpp}`、`ControllerInterfaces.h`、`ViewInterfaces.h`、`shell/MainWindow.cpp`；`src/app/CompositionRoot.{h,cpp}`；`tests/CMakeLists.txt`、`tests/core/CMakeLists.txt`、`tst_backend_registry.cpp`、`tst_cli.cpp`、`tst_document.cpp`、`tests/domain/tst_image_view.cpp`、`tests/infra/RecipeTestSupport.h`、`tst_recipe_store.cpp`、`tests/ui/tst_params.cpp`、`tst_presenter.cpp`；`docs/ux/copy.json`（只新增 key）；`perf/budgets.json`；`AI_CONTEXT.md`、`README.md`、`docs/architecture.md`、`docs/decisions.md`、`docs/current-status.md`、`docs/adr/014-*`、`015-*`、`docs/plan/S08`–`S12`、`.ai/handoff.md`

### 驗證
- `scripts/dev/test.ps1`：Release 45、Release＋OpenCV 50、Debug 45、Debug＋OpenCV 50 個 CTest 項目全過（Release＋OpenCV 與 Debug＋OpenCV 在最終程式上重跑過；Release、Debug 在加角度掃描測試之前跑過，那兩個組態不建置 `tests/backend/`，之後改的只有 OpenCV 的搜尋器與它的測試）。
- `node tools/check-deps.mjs`：173 個檔案 0 違規；`--self-test` 15 個 fixture 全過。`node tools/ste-lint.mjs --strict`：0 錯誤。`diagram.md` 7 張 Mermaid 用 mermaid 11（jsDelivr，瀏覽器窗格）解析全過，故意寫壞的一張被抓到。`explainer.html` 在瀏覽器窗格：每個控制項操作過、0 個 JS 錯誤、375 px 寬沒有水平捲動。
- 效能：`-Preset release-opencv -Perf` 連跑 3 次全過（8 個 perf 執行檔；`AOI_REAL_IMAGE_DIR` 有設）。T1、T2a、T4、T6a、T6b 3 次都在預算內；U3 仍超標（S04 起）。
- 變異：14 個，抓到 13 個；M11（拿掉角度的爬坡）存活，加了 ±5°、±30° 的角度掃描仍存活（結果完全相同）。
- `AiInspTool.pro`：Qt 6.12.0 的 qmake ＋ jom（Release）建置成功；offscreen 下 `--backend opencv` 結束碼 2、訊息說要用 `AOI_WITH_OPENCV=ON`。**沒有在 Qt Creator 裡載入過。**

### 未完成
- 流程 B 的 UI（使用者決定之後再接）；`Backend` 沒有 `libraryMatcher` 欄位。
- 真實影像上的門檻與晶格問題（使用者決定）；T7 的預算（提案）。
- HALCON（S08）；契約測試的泛化（S09）。

### 注意事項
- **T6b 要扣掉樣本框中心與元件中心的固定偏移**（匹配的參考點是使用者框的中心）；不扣的話傾斜與抖動的板子多 0.3–0.6 px。
- 角度誤差約是第 0 層步進的 0.2 倍（NCC 對角度的曲線在真值兩側不對稱）；試過「每個角度用自己的次像素峰值」只改善 0.013°，沒採用。
- 真實影像的路徑在 `.ai/handoff.md`；**不要把影像或疊圖加進版控**。
- 寫程式時又踩到：bash 的 `node -e` 與 heredoc 會吃掉反斜線（`\n` 變成真的換行、`build\release` 的 `\r`）；含反斜線的編輯改用 Write 寫腳本檔再執行。
- `.claude/launch.json`（為了在瀏覽器窗格跑 Mermaid 檢查暫時建的）已刪除。

## 2026-10-06 — Claude Code (Claude Opus 5.5)（S06：影像緩衝、轉接器、配方與樣本庫儲存）

### 完成
- 依 Step Protocol 做 S06，在 worktree 的分支 `step/06-image-storage`（從 `main` 的 `79aa74b` 開出）。
- 影像的所有權規則定案（ADR-014）：像素交出去就不改、owner 讓它活著、子影像共用 owner、做不到零複製就計數；`Bgr24`、`Gray16`、`subView`；`ImageBuffer`（緊密排列、對齊 64 byte）；`countImageCopy`（全程式共用的 atomic，測試取差）。
- 新 adapter `infrastructure/image_qt`（Qt Core＋Gui）：`fromQImage`／`wrapAsQImage`／`toCanonicalQImage`、`QtImageSource`。`check-deps` 新增 `qt-gui` 類別（白名單）與這一層的規則，2 個新 fixture（共 15 個）。開工時向使用者說明了為什麼不用精靈產生的 `mainwindow.*`、為什麼要新的一層。
- `backend_opencv`：`OpenCvImages`（`fromMat`、`borrowAsMat`；拒絕包著外部記憶體的 Mat）、`OpenCvImageSource`（自己讀 UTF-8 路徑再 `imdecode`）。**OpenCV 4.9.0（vc16）第一次連結成功**；preset `release-opencv`／`debug-opencv`，`aoi.exe` 建置後複製 DLL，測試的 PATH 加上 OpenCV。
- 影像來源只交出三種正規格式（`Gray8`、`Gray16`、`Bgrx32`）；組裝順序 **Qt 在前、OpenCV 在後**（計畫寫 Qt 當後備；改的理由是複製次數，ADR-014）。開圖改走 `IImageSource`，訊號帶 `ImageView`；`ImageLoader`、`ImageViewAdapter` 刪除。
- **12 bit**（使用者中途指出影像也有 12 bit）：`ImageView::significantBits`、`domain/image_depth`（看最大值推測）、畫布轉 8 bit 顯示（計數）、use case 的對比檢查依位元數換算、參考影像存位元數、合成板子產生 12 bit。實測 OpenCV 讀緊密打包的 12 bit TIFF 會左移 4 位。
- storage（Qt Core）：`JsonCodec`（每個欄位都寫、讀得嚴格、錯誤帶 JSON 路徑）、`JsonFile`（格式標記、`schemaVersion`、逐版遷移、`QSaveFile` 原子寫入、略過 BOM）、`JsonRecipeStore`、`JsonLibraryStore`（一個檔案一層、id 永不重複使用、壞檔不覆蓋）、`LayeredLibraryStore`（**使用者決定照 TRI 分層**：專案 → 使用者 → 唯讀的系統；先另開 session 圖解我的建議與 TRI 的差異）。
- application：`ImageSourceChain`、`RecipeSaver`（「非同步寫檔」：同一個檔案依序寫、還沒開始的舊存檔被取代）。
- `tests/support/synthetic_board`（同 seed 同輸出、標準答案 JSON、PNG＋JSON 存檔；不傾斜時鎖住像素雜湊）。
- **Qt 6.12.0**：使用者中途更新 Qt，選擇 S06 就切換。preset 與 `env.ps1` 的預設改成 6.12.0；切換前在 6.8.0 跑了一次全部效能測試當對照，切換後同樣跑一次（`perf.md`）。6.12.0 棄用了 `QDragEnterEvent(QPoint…)`，`tst_shell` 改用 `QPointF`。
- 文件：`docs/steps/S06-image-storage/`（四種輸出＋`perf.md`）、ADR-014、ADR-015、`perf/baseline/casparhsu1-storage.json`、`docs/architecture.md`、`AI_CONTEXT.md`、`README.md`、`docs/plan/S07`–`S12` 的「S06 完成後的提醒」。

### 修改的檔案
- 新增：`src/domain/image_buffer.*`、`image_copy.*`、`image_depth.*`；`src/application/image/ImageSourceChain.*`、`document/RecipeSaver.*`；`src/infrastructure/image_qt/`、`storage/`、`backend_opencv/`；`src/presentation/canvas/ImageViewMetaType.h`；`tests/support/`；`tests/infra/`（含 `fixtures/recipe-v1.json`、`library-v1.json`）；`tests/domain/tst_image_view.cpp`；`tests/core/tst_image_chain.cpp`、`tst_recipe_saver.cpp`；`tests/core/fixtures/deps/image-qt-widgets/`、`storage-gui/`、`clean/src/infrastructure/image_qt/`；`docs/steps/S06-image-storage/`；`docs/adr/014-*`、`015-*`；`perf/baseline/casparhsu1-storage.json`
- 修改：`src/domain/image_view.h`、`CMakeLists.txt`；`src/application/ports/IImageSource.h`、`IRecipeStore.h`、`ILibraryStore.h`、`usecases/GenerateObjectsFromSample.cpp`、`CMakeLists.txt`；`src/infrastructure/CMakeLists.txt`；`src/presentation/shell/MainWindow.*`、`presenters/AutoRoiPresenter.*`、`canvas/ImageItem.*`、`canvas/ImageCanvas.h`、`CMakeLists.txt`；`src/app/CompositionRoot.*`、`main.cpp`、`CMakeLists.txt`；`CMakeLists.txt`、`CMakePresets.json`、`cmake/AoiTargets.cmake`、`scripts/dev/env.ps1`、`scripts/dev/test.ps1`；`tools/check-deps.mjs`；`tests/CMakeLists.txt`、`tests/core/CMakeLists.txt`、`QtTestPrinters.h`、`tst_generate_objects.cpp`；`tests/domain/CMakeLists.txt`；`tests/ui/PresenterRig.h`、`perf_ui_baseline.cpp`、`tst_presenter.cpp`、`tst_scene.cpp`、`tst_shell.cpp`；`AiInspTool/AiInspTool.pro`；`AI_CONTEXT.md`、`README.md`、`docs/architecture.md`、`docs/decisions.md`、`docs/current-status.md`、`docs/plan/S07`–`S12`、`.ai/handoff.md`
- 刪除：`src/presentation/shell/ImageLoader.*`、`ImageViewAdapter.*`（測試搬到 `tests/infra/tst_image_qt`）

### 驗證
- **Qt 6.12.0（最終版）**：`scripts/dev/test.ps1` 四種組態全過：Release 45、Release＋OpenCV（`-Preset release-opencv`）47、Debug 45、Debug＋OpenCV 47 個 CTest 項目。切換前在 Qt 6.8.0（12 bit 加入前）四種組態也全過（44、46、44、46 個項目）；Debug 兩種組態在 6.8.0 第一次有失敗，原因見注意事項。
- `node tools/check-deps.mjs`：158 個檔案 0 違規；`--self-test` 15 個 fixture 全過。`ste-lint --strict`：698 句 0 錯誤。`check-contrast`：0 失敗。`s00-demo-fixture.mjs --check`：一致。`diagram.md` 8 張 Mermaid 用 mermaid 11.17.2（jsdom）解析全過，只驗語法；先確認一張故意寫壞的圖會失敗。`check-ai-structure.ps1`：OK（0 警告）。
- 效能（`perf.md`）：`perf-opencv`（6 個 perf 執行檔）在 Qt 6.12.0 連跑 3 次全過，切換前在 6.8.0 跑 1 次當對照；配方的量測另外單獨跑 3 次（查一個 483 ms 的異常值）。有預算的指標除了 U3 都在預算內；`perf/budgets.json` 沒有改。
- 變異：31 個，抓到 30 個（M05 第一輪存活，補了 EXIF 方向的測試後抓到；M24 是等價變異）；在 Qt 6.8.0 的 `release-opencv` 建置上做，還原後用 SHA-256 確認。
- `AiInspTool.pro`：Qt 6.12.0 的 qmake ＋ jom（Release）建置成功；offscreen 下 `--backend opencv` 結束碼 2、`--demo 500` 啟動後仍在執行。**沒有在 Qt Creator 裡載入過。**
- `explainer.html`：S06 中在瀏覽器窗格操作過各個控制項；最後一次改資料（6.12.0 的數字）之後，只確認了載入、表格內容、沒有主控台錯誤、375 px 寬沒有水平捲動。

### 未完成
- HALCON 的 `HImage` ⇄ `ImageView` 與 `HalconImageSource`：這台機器沒有 HALCON 的 C++ 開發檔，沒有做（S08）。
- 存檔與開啟配方的 UI（使用者決定這一步不接；必要功能，記在待辦）；開啟配方還需要 v2 schema（影像路徑）。
- 分層樣本庫的組裝（各層的檔案放哪裡）；有效位元數的覆寫（配方或相機設定）。
- 灰階＋alpha 的 PNG 兩個來源是否一致沒有驗證；`Gray16` 的畫布繪製成本沒有量；開圖仍在 UI 執行緒。

### 注意事項
- **Qt 6.8.0 的 `qUncompress` 對被截斷的資料會撞上 `Q_UNREACHABLE`**（Debug 中止；Release 是未定義行為，只是碰巧沒事）。Debug 的 `tst_recipe_store` 抓到；參考影像因此改成不壓縮的 base64。Release 碰巧通過，再次證明 Debug 要跑。
- Debug＋OpenCV 第一次跑時有兩個「失敗」是時序造成的：建置開始時二進位檔還是舊的編碼器，範例檔在測試前已改成新格式。四種組態全部重建後全過。
- `tst_image_sources_agree` 的「16 階灰色表的 PNG」原本預期兩邊不一致（`QEXPECT_FAIL`），實測一致（XPASS）：Qt 的 `isGrayscale()` 對索引色的定義是「索引 ＝ 灰階值」，不是「每個顏色都是灰」。真正不一致的是 16 階灰色表的 **BMP**（OpenCV 的 BMP 解碼器看「每個顏色都是灰」），改成記住它。
- `-0.0` 存成 JSON 讀回是 `+0.0`；Qt 的大小寫折疊是簡單折疊（`ß` ≠ `ss`）；Windows 上同時存同一個檔案，有的換名會失敗（30 次約 7 次）：都寫進契約或註解。
- 寫文件與程式時踩到的坑：bash heredoc 會吃掉反斜線（`/\s+/` 變 `/s+/`，`check-deps` 的自我測試抓到）；JS 的 `String.replace` 會把替換字串裡的 `$$` 變成 `$`（`.pro` 的 `$$files` 被改壞，看 diff 發現）；`for (x : f().value())` 在 C++20 是懸空參照（測試讀到空的）。
- 變異測試腳本與 Mermaid 檢查腳本在 session 的暫存目錄，不在 repo；變異的還原用原始位元組並驗證 SHA-256。
- `perf_storage` 把測試影像（約 250 MB）寫在**目前目錄**的 `perf-images-s06/`：CTest 在建置目錄執行所以沒事，手動執行時要先切到建置目錄（這個 session 在 worktree 根目錄跑過一次，已刪掉）。

## 2026-10-06 — Claude Code (Claude Opus 5.5)（S05：晶格估計與補漏）

### 完成
- 依 Step Protocol 做 S05，在 worktree 的分支 `step/05-lattice`（從 `main` 的 `4097b02` 開出；使用者的提示寫 `5c8923f`，但 `main` 之後已多了〔適配寬度〕的合併）。
- `src/domain/lattice/`（domain 第一次有 .cpp，只用標準庫；`aoi_domain` 由 INTERFACE 改 STATIC）：`LatticeEstimator`（kNN 位移分群找 u、v，RANSAC 挑原點，範圍逐次加倍的逐步精修，連通，Lagrange 約化；`fitAll` 依序找多組）、`LatticePredictor`、`LatticeAccumulator`（O(1) 正規方程）、`NonMaxSuppression`（旋轉矩形的精確重疊＋空間雜湊）、`SpatialGrid`、`IVectorClustering`。
- 接受規則（ADR-013，寧可沒有晶格也不要錯的晶格）每一條都對應一個測試重現過的失敗：散佈的點湊成晶格、三排間距不同湊成假的二維、4 px 雜訊下「每隔一格」的粗晶格沒被標出、斜著穿過第二列的一排（說明頁的情境抓到）、一排加一個離群點、兩個同間距的陣列被接成一個。
- use case：`SimpleGrid` 刪除；種子先 NMS；缺格與已有候選重疊不補；由近到遠、上限內留近的；驗證找到的元件用 O(1) 重新擬合，推算的位置用修正後的。`presentation/` 沒改。
- T3 第一版 p95 3.3–3.6 ms 超標；分段計時後兩輪改寫熱迴圈（平方距離、先算好反矩陣的指派器、`fastRound`、密集的格子表、逐列掃描的連通、v 先從鄰居方向取）降到 0.48 ms；最終 3 次 p95 0.65–1.19 ms。
- 正確性：情境掃描 1 029 組；與 S00 示範頁的 JavaScript 交叉驗證 36 組（`tools/s00-demo-fixture.mjs` 產生 fixture，CTest 檢查是否過期）；說明頁的晶格資料由 C++ 匯出，CTest 檢查是否過期。
- 變異測試 32 個，最終全被抓到：第一輪 17 個抓到、6 個存活（規則彼此備援）、9 個無法編譯；補了 `tst_lattice_rules`、`tst_lattice` 的斷言、use case 的兩處測試，第二輪 15 個全抓到。
- 修了 S04 的 `tests/ui/tst_presenter.cpp` 兩個時間相依的斷言（見注意事項）。
- 文件：`docs/steps/S05-lattice/`（四種輸出＋`perf.md`）、ADR-013、`perf/baseline/casparhsu1-lattice.json`、`perf/budgets.json`（T3 `calibrated`、`searchCalibrationS05`，limit 沒改）、`docs/plan/S06`、`S07`、`S10`、`S11` 開頭的「S05 完成後的提醒」。

### 修改的檔案
- 新增：`src/domain/lattice/`（23 個檔案）、`tests/domain/`（`CMakeLists.txt`、`LatticeTestSupport.h`、`tst_lattice_scenarios`、`tst_lattice_parts`、`tst_lattice_rules`、`tst_nms`、`tst_lattice_crosscheck`、`tst_lattice_explainer`、`perf_lattice`、`fixtures/s00-demo-lattice.json`）、`tools/s00-demo-fixture.mjs`、`docs/steps/S05-lattice/`、`docs/adr/013-lattice-acceptance-rules.md`、`perf/baseline/casparhsu1-lattice.json`
- 修改：`src/application/usecases/GenerateObjectsFromSample.{h,cpp}`、`src/application/CMakeLists.txt`、`src/domain/CMakeLists.txt`、`tests/CMakeLists.txt`、`tests/core/CMakeLists.txt`、`tests/core/tst_generate_objects.cpp`、`tests/core/perf_core.cpp`（標籤）、`tests/ui/tst_presenter.cpp`、`AiInspTool/AiInspTool.pro`、`perf/budgets.json`、`AI_CONTEXT.md`、`docs/architecture.md`、`docs/decisions.md`、`docs/current-status.md`（S02、S03 段落濃縮，行數回到上限內）、`docs/plan/S06`、`S07`、`S10`、`S11`、`.ai/handoff.md`
- 搬移：`tests/core/tst_grid.cpp` → `tests/domain/tst_lattice.cpp`（S03 的規格保留並擴充）
- 刪除：`src/application/usecases/SimpleGrid.{h,cpp}`

### 驗證
- `scripts/dev/test.ps1`：Release 與 Debug 各 36 個 CTest 項目全過。Debug 第一次全套有 1 個失敗（`tst_presenter`，見注意事項），修正後重跑全過。
- 情境掃描：主張的範圍內 420 組全部完全正確，範圍外 609 組的失敗都安全；交叉驗證 36 組一致；變異 32 個全抓到（兩輪後用 SHA-256 確認 24 個原始檔與變異前相同）。
- `node tools/check-deps.mjs`：133 個檔案 0 違規；`--self-test` 13 個 fixture 全過。`ste-lint --strict`：615 句 0 錯誤。`check-contrast`：0 失敗。`s00-demo-fixture.mjs --check`：一致。`diagram.md` 6 張 Mermaid 用 mermaid 11.17.2（jsdom）解析全過，只驗語法；先確認一張故意寫壞的圖會失敗。`check-ai-structure.ps1`：OK（0 警告）。
- 效能：`perf_lattice` 與 `perf_core` 各連跑 3 次（Release，`perf.md`）：T3 p95 0.65–1.19 ms（預算 2 ms）。
- `AiInspTool.pro`：qmake ＋ jom（Release）建置成功；offscreen 下 `--backend opencv` 結束碼 2、`--demo 500` 啟動 4 秒仍在執行。**沒有在 Qt Creator 裡載入過。**
- `explainer.html` 在瀏覽器窗格（本機 HTTP）操作過：各個特殊情況、速度表；375 px 寬沒有水平捲動（窄寬度的表格欄寬有調整）。窗格這次是深色外觀；淺色這次沒有再看。

### 未完成
- T3 的預算沒有說是哪一種 1 000 點：散佈的點 p95 3.1–4.2 ms、缺件加離群點 p95 1.6–2.1 ms 不在預算裡；要不要算進 T3，由使用者決定。
- 晶格的門檻要用真實影像校準（S07）；非矩形陣列（L 形、環形）沒有測；信心與「建議檢查」沒有顯示在畫面上（沒有任何 plan 檔寫到）；多回合的漸進驗證等 S07 的數字。

### 注意事項
- 規則彼此備援：只看估計器的結果，拿掉一條規則常看不出差別（變異測試第一輪 6 個存活就是這樣）。改規則時跑 `tst_lattice_rules`，它直接呼叫內部的規則函式。
- `tests/domain/fixtures/s00-demo-lattice.json` 與 `explainer.html` 的 `lattice-data` 是產生出來的，不要手改（各自的 CTest 會說怎麼重新產生）。
- `tst_presenter` 的時間假設：測試台的忙碌門檻 40 ms、最短顯示 120 ms；搜尋超過 40 ms 時，「進入預覽的瞬間」狀態列還是「搜尋中…」（依設計）。S04 的兩個斷言在那一瞬間讀狀態列；S05 讓 Debug 從放開滑鼠到預覽變成 49–72 ms（含輪詢誤差；Release 26–44 ms），全套跑出一次失敗。60 ms 的固定延遲可以穩定重現（3／3 失敗），改成 `QTRY_VERIFY2` 後 Release、Debug 各 3／3 通過。沒有量 S04 版程式在 Debug 的時間。
- 量測完之後拿掉了 `perf_lattice.cpp` 裡一個無效的檢查（拿兩個用同樣步驟建的累加器互比）；被量的程式沒變，事後跑一次 T3 p95 0.73 ms。
- 這台機器同一份程式會飄 1.4–2 倍（這次 `perf_core` 第 3 次比第 1 次慢約 2 倍）。
- Mermaid 的檢查腳本與變異測試腳本都在 session 的暫存目錄，不在 repo。

## 2026-10-06 — Claude Code (Claude Opus 5.5)（S04 之後：〔適配寬度〕）

### 完成
- 使用者看到正方形影像載入後左右有大片空白，要求加「適配寬度」。新增 `ImageCanvas::fitWidth()` 與 `FitMode`（`None`／`Window`／`Width`）：寬度撐滿畫布（左右留 `fitMarginPx`），保留畫布中央那一列但上緣、下緣不離開邊界，影像比畫布扁時垂直置中；視窗大小改變時沿用同一列；縮放或平移後不再跟著。`isFitted()` 的意義不變（只代表適配視窗）。
- `MainWindow`：〔↔ 適配寬度〕放在「檢視」選單與上方工具列，快捷鍵 `W`，沒有影像時停用。`copy.json` 新增 `action.fitWidth`；`ux-spec.md` 第 8 節加一列。載入影像的預設仍是適配視窗（規格沒改）。
- 分支 `feat/fit-width`（從 `main` 開出）；使用者看過結果後同意 commit，並以 `--no-ff` 合併到 `main`（只在本機，沒有 push，沒有 tag）。

### 修改的檔案
- `src/presentation/canvas/ImageCanvas.{h,cpp}`、`src/presentation/shell/MainWindow.{h,cpp}`、`docs/ux/copy.json`、`docs/ux/ux-spec.md`、`tests/ui/tst_canvas.cpp`、`tests/ui/tst_shell.cpp`、`.ai/handoff.md`

### 驗證
- Release 與 Debug 各 29 個 CTest 項目全過；`tst_canvas` 新增 7 個測試、`tst_shell` 新增 1 個並擴充 3 個。
- 變異測試 8 個全被抓到（resize 重新讀中央列會偏移、沒有夾住上下緣、扁影像沒有置中、平移後還在適配、resize 不跟著適配、少了 `W`、沒有影像也能按、按鈕接到適配視窗）；還原後雜湊相同。
- `check-deps` 0 違規、`ste-lint --strict` 0 錯誤、`check-contrast` 0 失敗。
- 沒有跑 qmake ＋ jom（沒有新增檔案，`.pro` 用萬用字元）；沒有效能量測（只改適配時的轉換，繪製路徑沒動）；沒有在真實視窗裡手動操作過。

### 未完成
- 無。載入影像要不要記住上次的適配方式，沒有做（規格是載入時適配視窗）。

### 注意事項
- 適配寬度在 resize 時**不能**重新讀視窗中央的影像列：畫布的 resize 錨點是 `NoAnchor`，捲動值不變而視窗變大變小，重新讀的話每次 resize 都會偏一點（變異 F01 證明測試抓得到）。

## 2026-10-06 — Claude Code (Claude Sonnet 5.5)（S04：Presenter、互動狀態機、復原／重做）

### 完成
- 依 Step Protocol 做 S04，在隔離的 worktree 裡建立 `step/04-presenter`。基準：S03 的程式在這個 worktree 重新建置、全部測試通過。
- 互動規則放在沒有 Qt 的 application 層（ADR-011）：`InteractionStateMachine`（8 狀態、19 事件）、`InspectionSession`、`ProposalSession`（memento 式復原）、`InspectionDocument`（5 種 Command）、`UndoStack`（Command 不可變、深度 200、乾淨位置）。domain 新增 `box_expander.h`；infrastructure 新增 `QtThreadPoolExecutor`。
- presentation：`AutoRoiPresenter`（世代丟棄遲到的結果、忙碌提示、取消立即回 Idle）、`ParamsPresenter`（防抖）、`SceneBuilder`、`ObjectsModel`／`ObjectsPanel`、`ParamsPanel`、`MessageTexts`、`AppPresenters`；`MainWindow` 變純 View；`SelectTool` 拖曳移動與方向鍵微調；`ShellController` 移除。
- 量到 S02 的繪製假設不成立（「虛線與點線的框很少」）：預覽時拖分數門檻，上千個框一次變成點線，一個畫格要 24–289 ms。改成填色矩形（ADR-012），點線畫格由 289 ms 降到約 31 ms。
- 量到 U2 的 98 ms 瓶頸是物件表欄位的 `ResizeToContents`；改成固定寬度後約 6 ms。
- 變異測試：植入 41 個缺陷，最終全被抓到（過程在 `perf.md`）。第一輪 37 個中 31 個第一次抓到；補測試與換寫法後其餘抓到，其中 M18（遲到的成功結果）第一次補的測試仍抓不到，原因是測試台的假 back end 一看到取消旗標就回 `Cancelled`。
- `docs/steps/S04-presenter/`（四種輸出＋`perf.md`）、ADR-011、ADR-012、`perf/baseline/casparhsu1-presenter.json`；`perf/budgets.json` 標註 U2、I3、I4、I6 `calibrated`、U3 `measured-over-limit`（limit 沒改）；`docs/plan/S05`–`S07`、`S10`、`S11` 開頭各加一行「S04 完成後的提醒」。

### 修改的檔案
- 新增：`src/domain/box_expander.h`、`src/application/{document,interaction}/`、`src/infrastructure/threading/QtThreadPoolExecutor.*`、`src/presentation/{presenters,objects,params}/`、`src/presentation/shell/ImageViewAdapter.*`、`tests/core/`（`tst_undo_stack`、`tst_state_machine`、`tst_box_expander`、`tst_proposal_session`、`tst_document`、`tst_session`、`tst_qt_executor`、`ProposalTestSupport.h`、`QtTestPrinters.h`）、`tests/ui/`（`tst_presenter`、`tst_objects_model`、`tst_params`、`tst_messages`、`tst_scene`、`PresenterRig.h`、`PerfSupport.h`、`perf_ui_s04`）、`docs/steps/S04-presenter/`、`docs/adr/011`、`012`、`perf/baseline/casparhsu1-presenter.json`
- 修改：`src/presentation/canvas/`（`BoxFrames`、`OverlayStyle`、`RoiLayerItem`、`ImageCanvas`、`ICanvasController`）、`src/presentation/shell/`（`MainWindow`、`MessageBar`）、`src/presentation/tools/`、`src/domain/geometry.h`（Windows 的 `min`／`max` 巨集）、`src/app/`（`CompositionRoot`、`main.cpp`）、各層的 `CMakeLists.txt`、`tests/ui/`（`tst_tools`、`tst_items`、`tst_shell`、`TestSupport.h`、`perf_paint_micro`）、`perf/budgets.json`、`docs/ux/ux-spec.md`、`docs/ux/copy.json`、`docs/architecture.md`、`docs/decisions.md`、`docs/current-status.md`、`docs/plan/S05`–`S07`、`S10`、`S11`、`AI_CONTEXT.md`、`README.md`、`AiInspTool/AiInspTool.pro`（只改註解）、`.ai/handoff.md`
- 刪除：`src/presentation/shell/ShellController.*`

### 驗證
- Release：`scripts/dev/test.ps1` 29 個 CTest 項目全過。Debug 也是 29 個全過（第一次有 1 個失敗：`tst_proposal_session` 的測試把指標指進暫存的 `objects()`，Release 碰巧通過，是測試的錯；已改成指向活著的物件後重跑）。
- `node tools/check-deps.mjs`：112 個檔案 0 違規；`--self-test` 13 個 fixture 全過。`ste-lint --strict`：549 句 0 錯誤。`check-contrast`：0 失敗。`diagram.md` 9 張 Mermaid 解析全過（只驗語法）。`check-ai-structure.ps1`：OK（0 警告）。
- 效能：`perf_ui_s04` 連跑 3 次（`perf.md`）：U2、I3、I4、I6 在預算內，**U3 超標**（5 832 個元件 p95 46–61 ms，預算 16 ms）。
- `AiInspTool.pro`：命令列 qmake ＋ jom（Release）建置成功；`AiInspTool.exe --backend opencv` 結束碼 2，`--demo 500` 啟動 4 秒仍在執行；沒有在 Qt Creator 裡載入過。
- `explainer.html` 在瀏覽器窗格實際操作過（狀態機、復原堆疊的接受／復原／重做流程、數字卡、窄寬度）；深色外觀沒有驗證（窗格對本機文件固定淺色）。

### 未完成
- U3 超標，處置由使用者決定（降低成本或重新定義預算）。預算數字沒有改。
- `LibraryPresenter`、`TypeEditorPresenter`（樣本庫面板、元件類型編輯器、流程 C）沒有任何 plan 檔涵蓋，需要使用者決定放進哪一步。
- 沒有量真實視窗；沒有量 Debug；文件沒有存檔（S06）；流程 B 的比對（S07）。

### 注意事項
- 變異測試腳本沒有留在 repo。還原要用 `WriteAllBytes` 並驗證雜湊：`Copy-Item` 會保留舊的修改時間，Ninja 不會重建，留下變異過的目標檔；一次還原失敗讓接著跑的兩個變異結果不可信，是靠雜湊檢查發現的。
- `tests/ui/PresenterRig.h` 的視窗閘門故意不看取消旗標（模擬不可中斷的函式庫呼叫），不要「修正」它。
- `perf_ui_s04` 的 `knownOver("U3")` 是已知超標的記錄，不是放寬預算；S11 處理 U3 後要拿掉。
- 這個 session 的 bash 工具有時無法解析 heredoc，`node -e` 會吃掉引號；產生腳本一律用 Write 工具寫檔再執行。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)（S03：系統核心）

### 完成
- 依 Step Protocol 做 S03，在隔離的 worktree 裡建立 `step/03-core`。基準：S02 的程式在這個 worktree 重新建置、5 個測試全過。
- domain 型別與座標慣例（`geometry.h`）、九個 port（含 S01 加的 `ILibraryStore`、`ILibraryMatcher`，契約寫在標頭）、`Result`／`Error`、`CancelSource`／`CancelToken`、`JobRunner`（latest-wins）、use case `GenerateObjectsFromSample` 與 `classify`、暫代的 `SimpleGrid`、`BackendRegistry`、組裝根與 `--backend`、假 back end、`PerfProbe`、`ThreadPoolExecutor`。
- `tools/check-deps.mjs`（掛 CTest）加 13 個 fixture 與 `--self-test`；`tst_cli` 從外面用真的行程驗證 `--backend`。
- ADR-003（Qt Test 統一）、ADR-010（座標與角度慣例，解決 ADR-006 未決 #5）；`docs/steps/S03-core/` 的四種輸出與 `perf.md`；`perf/baseline/casparhsu1-core.json`；`docs/plan/S04`–`S10` 開頭各加一行「S03 完成後的提醒」；`AiInspTool.pro` 同步納入新的層。
- 變異測試：植入 14 個缺陷，每一個都被對應的測試抓到（結果在 `perf.md`）。其中一個（`rotate()` 正負號反過來）只有 `tst_domain` 抓得到，因為其他測試的標準答案用同一個 `rotate()`。
- 量測發現：`PerfProbe` 關閉的成本量不出來；暫代的晶格擬合在約 4 900 點要 97–134 ms；取消延遲在 1 ms 輪詢的工作上是 Windows 計時器解析度的函數。
- 測試抓到／過程中踩到的錯：`explainer.html` 第一版把含單引號的錯誤訊息拼進 JavaScript 字串，在瀏覽器窗格才發現語法錯誤（改成走 JSON ＋ `esc()`）；`windows.h` 的 `min`／`max`／`near` 巨集弄壞 `perf_core`（改 `NOMINMAX`、改名）；Qt 在 Windows 上只有 stderr 是主控台時才寫 stderr，啟動錯誤原本用 `qCritical` 被管線接走就看不到（改寫到 stderr）。

### 修改的檔案
- 新增：`src/domain/`（`geometry.h`、`match.h`、`object_state.h`、`search_params.h`、`component.h`、`recipe.h`）、`src/application/`、`src/infrastructure/`（`backend_fake`、`perf`、`threading`）、`src/app/CompositionRoot.*`、`tools/check-deps.mjs`、`tests/core/`（含 `fixtures/deps/`）、`docs/steps/S03-core/`、`docs/adr/003`、`010`、`perf/baseline/casparhsu1-core.json`
- 修改：`src/domain/image_view.h`（新增 `umPerPixel`，預設 0）、`src/app/main.cpp`（`--backend`、啟動錯誤寫 stderr）、各層的 `CMakeLists.txt`、`tests/CMakeLists.txt`、`AiInspTool/AiInspTool.pro`、`AI_CONTEXT.md`、`README.md`、`docs/architecture.md`、`docs/decisions.md`、`docs/current-status.md`、`docs/plan/S04`–`S10`、`.ai/handoff.md`

### 驗證
- `scripts/dev/test.ps1`：Release 與 Debug 各 17 個 CTest 項目全過（5 個 UI、8 個核心測試執行檔加 `tst_cli`、3 個 `check_deps*`）；`/W4 /WX` 無警告。
- `node tools/check-deps.mjs`：80 個檔案 0 違規；`--self-test`：13 個 fixture 全過；`node tools/ste-lint.mjs --strict`：488 句 0 錯誤；`check-ai-structure.ps1`：OK。
- `diagram.md` 10 張 Mermaid 用 mermaid 11 的解析器（jsdom）解析全過，只驗語法；先用一張故意寫壞的圖確認解析器會失敗。
- `explainer.html` 在瀏覽器窗格（本機 HTTP 伺服器）實際操作：四張卡片的每個按鈕與滑桿、淺色與深色、窄寬度；靜態快照模式不執行 script，所以要用 HTTP。
- `perf_core` 連跑 3 次（Release，量測衛生同 S02）。`aoi.exe --backend=opencv|halcon|turbo` 結束碼 2、訊息正確；預設與 `--backend=fake` 能啟動並持續執行。
- `AiInspTool.pro`：獨立目錄 qmake ＋ jom（Release）建置成功。**沒有在 Qt Creator 裡載入過。**

### 未完成
- ADR-009（UI 工具集重審）仍等使用者決定，與 S03 無關。
- 流程 B 只有 port 與型別；`SimpleGrid` 等 S05 取代；假 back end 不看影像；OpenCV／HALCON 的座標與角度轉換（ADR-010）未驗證。

### 注意事項
- 這個 session 在隔離的 worktree（分支 `claude/trusting-colden-06cdda`），`main` 在使用者的主工作目錄被檢出，所以「合併到 main」要在主工作目錄執行 `git merge`；細節見本步驟的收尾訊息。
- 用 `node -e '…'` 或 heredoc 寫含反斜線、中文或單引號的字串會被 shell 弄壞（這次踩到兩次：反斜線被吃、換行被塞進字串）；一律用 Write／Edit 工具寫檔。PowerShell 5.1 讀 `.ps1` 不認無 BOM 的 UTF-8，腳本裡不要放中文字串。
- `String.prototype.replace` 的替換字串裡 `$$` 會變成單一個 `$`（`AiInspTool.pro` 的 `$$PWD` 因此被弄壞過一次，已修）；程式化修改檔案時用函式型的替換。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)（AiInspTool：qmake 專案，用 Qt Creator 開啟 S02 程式）

### 完成
- 使用者在 `AiInspTool/` 用 Qt Creator 建了 qmake 的 Qt Widgets 專案（第一次建的是 Qt Quick，建錯後砍掉重建）。把 `AiInspTool.pro` 改成直接建置 `../src` 的 S02 程式（domain、presentation、`app/main.cpp`，萬用字元納入，程式碼只有一份），新增 `aoi_ux.qrc` 內嵌 `tokens.json`、`copy.json`。精靈產生的 `main.cpp`、`mainwindow.*` 保留但沒有被使用。
- commit `fd899ce`（直接提交在 `main`，因為主工作目錄正開在 Qt Creator，切換分支會讓追蹤的檔案暫時消失）。清掉了使用者砍掉的舊檔留下的 intent-to-add 索引殘留（`CMakeLists.txt`、`Main.qml`）。
- 解釋了使用者的疑問：S 拖框只會顯示框與訊息列，**S02 還沒有建樣本與搜尋**（S04 起有假資料的完整流程，S07 才有真的搜尋）。

### 修改的檔案
- 新增：`AiInspTool/`（`AiInspTool.pro`、`aoi_ux.qrc`，以及使用者的精靈檔案 `.gitignore`、`main.cpp`、`mainwindow.cpp`、`mainwindow.h`、`mainwindow.ui`）
- 修改：`AI_CONTEXT.md`（目錄表）、`.ai/handoff.md`、`docs/changelog-ai.md`

### 驗證
- 在獨立的建置目錄用 qmake 加 jom 建出 `AiInspTool.exe`，沒有錯誤（55 秒）；離屏執行 `--demo 5000` 五秒沒有當機。
- **沒有在 Qt Creator 裡實際載入專案並執行**；Qt Creator 要手動「執行 qmake」才會讀到新的 `.pro`。

### 未完成
- 使用者的 `AiInspTool/.gitignore` 沒有涵蓋 `release/`、`debug/`、`.qtc_clangd/`，目前是未追蹤的建置產物（沒有納入 commit，也沒有改使用者的檔案）。
- S03 起 `src/` 新增 application、infrastructure 時，`AiInspTool.pro` 要跟著調整。

### 注意事項
- 這個 session 在隔離的 worktree，編輯工具被護欄擋在主工作目錄之外；放進 `AiInspTool/` 的兩個檔案是經使用者同意後用 `Copy-Item` 複製的（複製前確認使用者的 `.pro` 與備份相同）。
- `AiInspTool/` 不在五層之內，`tools/check-deps.mjs`（S03）不要掃它。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)（S02：Qt UI 骨架與影像畫布）

### 完成
- 依 Step Protocol 做 S02：CMake＋presets、`src/` 三層（domain 只有 `ImageView`）、主視窗與 dock 空殼、影像畫布（游標中心縮放、平移、適配、零複製）、批次繪製層與單一物件 Item、四個工具與註冊表、暫代 controller、固定高度的訊息列、tokens → QPalette／QSS、文案直接讀 `copy.json`、`FakeScene`、5 個 Qt Test 單元測試執行檔、效能基準線，以及四種輸出與 `perf.md`。
- 第一件事（工具鏈驗證）：VS 2022 17.14／`cl` 19.44、Qt 6.8.0、CMake 3.30.5、Ninja 1.12.1 實際 configure、build、執行；寫回 `AI_CONTEXT.md` 環境表。發現 HALCON 只有 runtime（沒有標頭與程式庫）、OpenCV 的 `OpenCV_DIR` 要指到 `build\x64\vc16\lib`。
- 效能：第一版（描邊）I1 p95 約 50 ms，量測找到瓶頸是描邊的光柵化，改成整數像素的填色細矩形（純 QPainter 微基準約快 4 倍）。U1a、U1b、U4、I2、I5 通過；I1 在預算邊緣（offscreen 13 次執行 p95 14.1–20.7 ms）、真實視窗 22.1 ms 超標。預算數字沒改。
- 更正原本的假設：批次繪製**不是** 5 000 框變順的主因（Item 不多時對平移縮放幀沒有幫助），價值在第一幀與 Item 很多時的 p95。
- 測試抓到：Rejected 點線原本完全畫不出來（Qt 不畫長度 0 的虛線段）；變異測試確認三個關鍵行為的測試有效。
- 發現並處理兩個環境問題：效能量測會被混合架構 CPU 的核心與電源狀態影響 2 倍（`MeasurementHygiene.h`）；繁中 Windows 下字碼頁不一致會讓 Ninja 掉標頭相依（`env.ps1` 對齊字碼頁）。
- 新增 ADR-007（文案來源）、ADR-008（訊息列固定高度）、ADR-009（待使用者決定：S02 實測後重審 UI 工具集）；ADR-001 補了實測結果。

### 修改的檔案
- 新增：`CMakeLists.txt`、`CMakePresets.json`、`cmake/`、`src/`、`tests/ui/`、`scripts/dev/`（`env.ps1`、`test.ps1`）、`perf/baseline/casparhsu1.json`、`docs/steps/S02-ui-shell/`、`docs/adr/007`、`008`、`009`
- 修改：`AI_CONTEXT.md`（環境表、目錄、建置與測試指令）、`README.md`、`docs/architecture.md`（模組表）、`docs/decisions.md`、`docs/adr/001-ui-toolkit.md`（補充）、`docs/current-status.md`、`.ai/handoff.md`、`docs/ux/tokens.json`（`sizes.messageBarHeight`）、`docs/ux/copy.json`（新增殼層 key）、`docs/ux/ux-spec.md`（修改紀錄）、`perf/budgets.json`（校準狀態與實測值，limit 沒改）

### 驗證
- `scripts/dev/test.ps1`：Release 與 Debug 各 5 個測試執行檔全過；變異測試（縮放錨點、LOD 門檻、框夾限）都會被抓到。
- `node tools/ste-lint.mjs --strict`：434 句 0 錯誤；`node tools/check-contrast.mjs`：全過；`diagram.md` 4 張 Mermaid 用 mermaid 11 解析器全過（只驗語法）；`scripts/ai/check-ai-structure.ps1`：OK。
- 效能基準線：offscreen 全套（量測衛生開啟，多次執行）、真實視窗子集、純 QPainter 微基準，結果在 `docs/steps/S02-ui-shell/perf.md` 與 `perf/baseline/casparhsu1.json`。
- `-DAOI_WITH_HALCON=ON`：缺標頭時 configure 明確失敗；`-DAOI_WITH_OPENCV=ON`：找到 4.9.0（只 configure，沒有連結）。

### 未完成
- 等使用者決定 ADR-009（I1 與 UI 工具集）。
- 拖曳移動元件、方向鍵微調、設定記憶、減少動態、高 DPI 螢幕實測：沒有做。U2、U3、I3、I4、I6：S04。
- 暫代品：`ShellController`（S04）、`ImageLoader`（S06）、`ui.shell.*` 文案（S04）。

### 注意事項
- 一定要在 `scripts/dev/env.ps1` 之後 configure 與 build；Bash heredoc 會壓縮連續反斜線（寫含反斜線的內容用 Write／Edit）。
- 效能數字只在同一台機器、同樣的量測衛生下可比較；同一份程式不同時間會飄 ±30%，回歸閘門（S10）要考慮。
- `docs/ux/tokens.json`、`copy.json` 改了要重建（內嵌進執行檔）。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)（S01：UX 規格與可點擊原型）

### 完成
- 依 Step Protocol 做 S01（設計步驟，沒有 Qt 程式）：UX 規格、狀態語言 token（含顏色對比驗算工具）、文案（繁中＋英文）、ADR-001／002／006、可點擊原型，以及四種輸出與 perf.md。
- 使用者看原型後把範圍擴大：加入樣本庫與區域掃描（流程 B）、Board › Component › Box 階層、元件類型編輯器（矩陣、相依窗、套用範圍）、CAD 清單。對照了使用者提供的三份 TRI 文件。
- 使用者回饋「訊息蓋住料件」：所有要閱讀的訊息改放畫布下方的訊息列。
- 使用者決定庫與板子是複本關係、庫存 µm；原型隨之改成複本語意（〔從樣本庫更新〕、〔存入樣本庫〕）。
- 使用者找不到「存入樣本庫」的按鈕（原本只有 Enter 鍵、按鈕藏在 dock 下方）：預覽時在訊息列加上〔接受〕〔接受並存入樣本庫…〕〔捨棄〕，並修正說明文字的順序。
- 使用者問「樣本庫很多不是要比很久」：新增「比對範圍」（先比板子已有的類型，找不到再擴大）與狀態列「比對 N 個樣本」；預篩選等寫入 ADR-006 §7，未量測。

### 修改的檔案
- 新增：`docs/ux/`（ux-spec.md、tokens.json、copy.json）、`docs/adr/001-ui-toolkit.md`、`002-undo-model.md`、`006-sample-library-and-hierarchy.md`、`docs/steps/S01-ux-spec/`（spec.md、diagram.md、explainer.html、storyboard.md、perf.md）、`tools/check-contrast.mjs`
- 修改：`docs/decisions.md`、`docs/current-status.md`、`.ai/handoff.md`、`AI_CONTEXT.md`、`docs/00-master-plan.md`、`perf/budgets.json`（記錄 UI 預算確認與三點定義）、`docs/plan/S02`–`S10`、`S12`（開頭各加一行「S01 修訂」）

### 驗證
- `node tools/ste-lint.mjs --strict`：386 句，0 錯誤，0 警告；負向測試（長的被動句）會失敗。
- `node tools/check-contrast.mjs`：淺色 18、深色 18、疊圖 24 組全過；負向測試（壞的 token）會失敗。
- `diagram.md` 的 12 張 Mermaid 圖用 mermaid 11 的解析器（jsdom）解析全過，只驗語法，沒有渲染；解析器對壞語法會失敗。
- 原型頁內腳本，對最終版：整套 124 項斷言全過，另有比對範圍約 20 項，主控台無錯誤。涵蓋流程 A／B／C、三個範圍的復原、複本語意、11 種原因中的 9 種無匹配、三種錯誤、忙碌 200 ms／進度 1 s 的時機、把手 24 px 點擊區正反面、CAD 清單、訊息不蓋在影像上、淺深色、繁中／英文、無非法轉移。
- 建置、單元測試、`check-deps`：不適用（尚未建立）。效能：全部未量測。

### 未完成
- ADR-006 未決項目（見 `docs/current-status.md`）。
- S02 由新的 session 做（Start prompt 在 `docs/plan/S02-ui-shell.md` 底部）。

### 注意事項
- 過程中抓到並修正的真實缺陷：接受後復原再重做會先重做殘留動作；畫面快取鍵漏看群組內容，編輯樣本庫後「與樣本庫不同」的提示不會出現；框到多個元件時的提示是錯的（舊提示說「沒框到元件」）。
- 測試失敗多半是測試腳本自己的錯（期望值、順序、沒先選取群組），已改正重測；報告裡的數字是改正後對最終版的結果。
- 真實滑鼠／鍵盤／滾輪的抽查只在第一版原型做過；新版是合成事件加截圖。
- `main` 已在另一個工作樹 checkout，合併要在那邊做。
- 使用者的 S01 提示寫「commit 範本在 plan 檔、用 `-m`」，與 `project-rules.md` 的格式規則衝突；問過一次，使用者沒回答格式，只說 commit。我選了 `project-rules.md` 的格式（`[Caspar]docs:`、`git commit -F` 讀檔），因為那是使用者稍早確認的；不合要求可以 amend（沒有 push）。
- 合併是在 `main` 的工作樹（`D:/AOI_Project/Qt/AutoCadCreated`）用 `git -C` 做的，因為 `main` 在那邊已 checkout；做之前確認它是乾淨的。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)

### 完成
- 依使用者決定，把 `ai-project-template/`（共用結構範本、套用腳本、設計說明，共 18 個檔案）納入版控，放在這個 repo 內。這個範本是同一個 session 依使用者「設計共用結構」的要求產生的，並非產品計畫的一部分。
- 更新文件中「未追蹤、不要 stage」的說法：`AI_CONTEXT.md`、`project-rules.md`、`current-status.md`、`handoff.md`。`current-status.md` 原本寫「非本計畫產生」，造成誰做的、為什麼做的看不出來，改成寫明來源。

### 修改的檔案
- 新增：`ai-project-template/`（18 個檔案）
- 修改：`AI_CONTEXT.md`、`.agents/harness/project-rules.md`、`docs/current-status.md`、`.ai/handoff.md`、`docs/changelog-ai.md`

### 驗證
- `git add -n` 預覽：18 個檔案，沒有被 ignore 的內容；掃描沒有密鑰或機器路徑（只有規則裡拿 `HQ\Caspar.Hsu` 當作者名字的範例）。
- `scripts/ai/check-ai-structure.ps1 -Strict`：通過，0 警告。
- `node tools/ste-lint.mjs`：206 句，0 錯誤（它只掃 `docs/`，不含範本）。
- 用 repo 內這份範本重新建立範例專案：建置腳本結束碼 0，結構檢查通過。

### 未完成
- 已合併的分支（`chore/ai-agent-structure`、`chore/commit-format`）與備份分支 `backup/before-commit-reformat` 還沒清。

### 注意事項
- 範本放在產品 repo 內但與產品無關。它底下的 `template/CLAUDE.md`、`template/AGENTS.md`、`template/.claude/settings.json` 是給「新專案」用的檔案，不是這個 repo 根目錄的設定；agent 在這個 repo 工作時，以根目錄的 `AGENTS.md`、`CLAUDE.md` 為準。
- 若日後要把範本搬離這個 repo，同步更新 `AI_CONTEXT.md` 等引用它的文件。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)

### 完成
- 把前三個 commit 的訊息改寫成使用者的 `[作者]屬性:` 格式（檔案內容、作者、時間不變；新編號 `fd4fd2a`、`13230f8`、`74272a2`）。改寫前的完整歷史留在分支 `backup/before-commit-reformat`。
- `project-rules.md` 的 Commit 規則由 Conventional Commits 改為該格式，並訂為唯一權威；Step Protocol 第 5 步同步調整。訊息一律寫成檔案再 `git commit -F`，`AI_CONTEXT.md` 的 shell 備註同步修改。
- ADR-000 補上這次格式變更的說明。
- 依使用者要求以 `--no-ff` 合併到 `main`，沒有 push。

### 修改的檔案
- `.agents/harness/project-rules.md`、`AI_CONTEXT.md`、`docs/adr/000-ai-agent-structure.md`、`docs/changelog-ai.md`、`.ai/handoff.md`、`docs/current-status.md`
- 範本 `ai-project-template/`（未追蹤，不在 commit 內）：`project-rules.md` 改成同一份預設，README 更正「commit 格式屬 AOI 專屬」的說法。

### 驗證
- `scripts/ai/check-ai-structure.ps1 -Strict`：通過，0 警告。
- `node tools/ste-lint.mjs`：206 句，0 錯誤。
- 用修改後的範本重新建立範例專案：建置腳本結束碼 0，結構檢查通過，新的 Commit 訊息一節已帶入。
- 新 commit 訊息以腳本逐項檢查格式（標題、`測試 :`、Co-Authored-By、結尾兩行、每行寬度）。

### 未完成
- 12 份 `docs/plan/S*.md` 的「Commit 範本」仍是英文 Conventional 風格，沒有改寫；`project-rules.md` 規定它們只當內容參考，格式以規則為準。
- 確認改寫後的歷史沒問題後，刪除備份分支 `backup/before-commit-reformat`。

### 注意事項
- 結尾的 `[Test]` 與 `Build OK, Test OK` 是使用者的固定規則；專案目前沒有建置，所以它不代表本次有建置。實際驗證只看 `測試 :` 區塊，且必須如實。
- 兩份規範衝突時（repo 內文件與使用者慣例），先問使用者，不要自己挑。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)

### 完成
- 把 AI 共用結構（Codex 與 Claude Code）套用到本專案，並與既有的 `CLAUDE.md` 合併：原內容搬到 `AI_CONTEXT.md`、`docs/architecture.md`、`.agents/harness/project-rules.md`，`CLAUDE.md` 改為薄入口。
- `docs/STATUS.md` 更名為 `docs/current-status.md`；14 個檔案（12 份 `docs/plan/S*.md`、`00-master-plan.md`、`S00-overview/spec.md`）的舊引用改成新路徑。
- 新增 `docs/adr/000-ai-agent-structure.md`；`docs/decisions.md` 作為 ADR 索引。
- `.gitignore` 補上 `.claude/worktrees/`、`CLAUDE.local.md`。

### 修改的檔案
- 新增：`AGENTS.md`、`AI_CONTEXT.md`、`.agents/`、`.claude/`、`.ai/handoff.md`、`scripts/ai/`、`docs/architecture.md`、`docs/decisions.md`、`docs/changelog-ai.md`、`docs/adr/000-ai-agent-structure.md`
- 改寫：`CLAUDE.md`、`.gitignore`
- 更名：`docs/STATUS.md` → `docs/current-status.md`
- 引用更新：`docs/plan/S01`–`S12`、`docs/00-master-plan.md`、`docs/steps/S00-overview/spec.md`

### 驗證
- `scripts/ai/check-ai-structure.ps1 -Strict`：通過，0 警告（Windows PowerShell 5.1）。
- `node tools/ste-lint.mjs`：206 句，0 錯誤，0 警告。
- 舊名稱（`STATUS.md`、`CLAUDE.md 的…`）區分大小寫掃描：除了刻意保留的歷史說明，沒有殘留。
- Claude Code 實測：`AI_CONTEXT.md`、`project-rules.md`（含 Step Protocol）、`AGENTS.md`、`architecture.md` 都經 `CLAUDE.md` 的 `@` 匯入載入。
- 未驗證：Codex 端。這台的 Codex CLI 0.136.0 無法執行 `codex exec`（解析模型清單失敗），所以 `AGENTS.md` 的必讀清單流程沒有實測。

### 未完成
- 變更已 commit（`fd4fd2a`），並依使用者要求以 `--no-ff` 合併到 `main`。沒有 push，是否推到遠端由使用者決定。
- `ai-project-template/` 仍在 repo 內且未追蹤，不在這次 commit，要放哪裡由使用者決定。

### 注意事項
- 分層規則原本每個 Claude session 都載入，所以 `CLAUDE.md` 額外匯入 `docs/architecture.md`（`AGENTS.md` 也列入必讀）。不要移除，否則護欄會變弱。
- 舊版 `CLAUDE.md` 在 git 歷史（commit `28414b1`）。
- `.claude/settings.json` 的 deny 規則在 Claude Code 信任此資料夾後才生效。
