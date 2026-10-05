# S02 — Qt UI 骨架與影像畫布：目的、原因、效果

> 規則：英文區塊用 ASD-STE100 風格（短句、主動語態、一詞一義）。繁中摘要放在每段之後。
> 檢查：`node tools/ste-lint.mjs --strict`（只檢查句長與被動語態，不是完整 STE 驗證）。
> 設計來源：[`docs/ux/ux-spec.md`](../../ux/ux-spec.md)、[`tokens.json`](../../ux/tokens.json)、[`copy.json`](../../ux/copy.json)、[ADR-001](../../adr/001-ui-toolkit.md)。本檔只講「為什麼這樣切」；實測數字在 [`perf.md`](perf.md)。

<!--STE-->
## Purpose
This step builds the main window and the image canvas.
The canvas zooms, pans and draws boxes on a 5 MP image.
It has no vision code yet.
<!--/STE-->

**繁中**：做出主視窗與影像畫布：游標中心縮放、平移、在 5 MP 影像上畫框。這一步還沒有任何視覺（搜尋）程式。

<!--STE-->
## Reason
The engineer works on the canvas all day.
A slow canvas costs time on every task.
So we make the canvas fast before we add any search.
<!--/STE-->

**繁中**：工程師整天都在畫布上工作，畫布慢就是每個任務都慢。所以在加入搜尋之前，先把畫布做到順，並且用數字證明。

<!--STE-->
## Effect
We have measured numbers for screen speed on this PC.
Five of the six speed budgets pass. The sixth budget, I1, sits at its limit.
We report the gap and the options. The user decides.
<!--/STE-->

**繁中**：我們有了這台機器上的畫面速度實測基準線（`perf.md`、`perf/baseline/`）。六個速度預算裡五個通過（U1a、U1b、U4、I2、I5，餘裕大）；第六個 I1（5 000 框的平移縮放）在預算邊緣，真實視窗超標。差距與選項寫在 [ADR-009](../../adr/009-ui-toolkit-review-after-s02.md)，由使用者決定。之後任何預算變慢，Release 組態的 `perf_ui_baseline` 會失敗（I1 是已記載的例外，只警告）。數字換機器會不同。

<!--STE-->
## Three parts of the canvas
The canvas has three kinds of part. An item draws. A tool reads the pointer. A controller receives the result.
An item draws boxes and knows nothing about tools.
A tool decides what a click means and knows nothing about the canvas widget.
A new tool is a new class that we register. We do not change the canvas.
<!--/STE-->

**繁中**：畫布有三種零件：Item 只負責畫（`ImageItem`、`RoiLayerItem`、`RoiGraphicsItem`）；Tool 決定「這個操作是什麼意思」（`SelectTool`、`SampleRoiTool`、`RegionScanTool`、`AddComponentTool`），只透過 `ToolEvent` 與 `IToolHost` 說話，不認識 `QGraphicsView`；Controller 接收結果（`ICanvasController`、`ISelectionController`，S04 由 Presenter 實作）。新增工具＝新增一個類別並註冊，不改畫布。

<!--STE-->
## Why the layer draws the boxes this way
We measured three causes of speed.
First, we draw each box as four thin filled rectangles. A stroked outline costs about four times more.
Second, the layer skips a pin or a window that is smaller than three pixels. The body box always stays.
Third, one layer starts faster than many items when the board has many boxes.
<!--/STE-->

**繁中**：量測找到三個讓畫布變快的原因（數字在 [`perf.md`](perf.md)）。**一、畫法**：每個框畫成上下左右四條整數像素的填色細矩形（`BoxFrames`），不用 cosmetic 描邊；純 QPainter 微基準 18–22 ms → 4.5–4.9 ms（2 600 個框），約 4 倍。**二、層級簡化**：螢幕上短邊 < 3 px 的 Pin 與窗直接略過，主體永遠畫；縮到 0.29 以下時 5 000 框只畫 160 個（適配 1000×700 視埠的縮放是 0.335，那時畫 2 580 個）。**三、批次**：`RoiLayerItem` 把同（狀態 × 種類）的框放進同一個桶、一次畫完；Item 很多時第一幀快很多（每元件一個框、20 000 框：154 vs 45 ms），平移縮放幀的 p95 也好很多（20 000 框：78.7 vs 38.0 ms）。**要誠實更正原本的假設**：Item 不多時（每個元件 41 個框、約 120 個 Item），批次對平移縮放幀幾乎沒有幫助；讓 5 000 框變順的主要是畫法與 LOD。需要個別互動的少數物件（拖曳中的框、類型編輯器的框）才用 `RoiGraphicsItem`。

<!--STE-->
## Messages stay under the image
The message bar sits under the canvas. It has a fixed height.
A long message scrolls inside the bar. It never makes the canvas smaller.
A message never covers the part on the board.
<!--/STE-->

**繁中**：所有要閱讀的訊息（提示、吐司、無匹配卡片、錯誤）都在畫布下方的訊息列，高度固定 96 px，放不下就在列內捲動；畫布大小不因訊息而變。唯一蓋在畫布中央的是「尚未開啟影像」卡片（那時畫布上沒有東西可擋）。S01 規格的「高度隨內容變化、畫布不變」在桌面程式只能這樣同時成立，見 [ADR-008](../../adr/008-message-bar-fixed-height.md)。

<!--STE-->
## One source for color and text
The colors and sizes come from one file, tokens.json. The text comes from one file, copy.json.
The program reads both files at run time from the embedded copy.
A test checks that every text key has both languages.
<!--/STE-->

**繁中**：顏色、線型、尺寸只有一個來源（`docs/ux/tokens.json`），文案只有一個來源（`docs/ux/copy.json`），都內嵌進執行檔；程式裡沒有再寫一份。`ThemeManager` 把 tokens 變成 `QPalette` 與 QSS（淺色、深色、跟隨系統；字級 100／125／150%）；疊圖配色固定亮色，兩個主題相同。`JsonTranslator` 讓程式用標準的 `qtTrId("key")` 取文案（[ADR-007](../../adr/007-copy-json-as-translation-source.md)）。`tst_theme_i18n` 逐 key 檢查兩種語言與佔位字一致。

<!--STE-->
## What this step does not do
This step does not search, build a model or undo an action.
A stand-in controller answers each action with a message.
Step S04 replaces the stand-in with a presenter.
<!--/STE-->

**繁中**：本步驟不做搜尋、不做資料模型、不做復原。`ShellController` 是暫代品，每個操作都有回應（訊息列、選取外框），但不接任何 use case；S04 的 Presenter 實作同樣的介面來取代它。

## SOLID 對照

| 原則 | 這一步的具體做法 | 驗證 |
|---|---|---|
| **S** | 每個工具一個類別；`RoiLayerItem` 只管批次繪製、`RoiGraphicsItem` 只管單一物件與把手；`MessageBar`、`EmptyCard`、`ShellController` 各一個責任 | `tst_tools`（每個工具單獨測）、`tst_items` |
| **O** | 新增工具＝新增 `ITool` 實作＋`ToolRegistry::registerTool`，不改 `ImageCanvas` | `tst_tools::registeringTheSameIdReplacesTheTool` |
| **L** | `ShellController` 與日後的 Presenter 都實作同一組 `ICanvasController`／`ISelectionController`；測試用 `RecordingController` 取代它也行 | `tst_tools`、`tst_shell`（兩種實作都走同一條路徑） |
| **I** | `ICanvasView`、`ICanvasController`、`IToolHost`、`ITool` 各 ≤ 5 個方法；工具只看 `ToolEvent` 這個值，不看整個畫布 | 程式審查（方法數） |
| **D** | 畫布透過 `ICanvasController` 對外；`ImageCanvas` 不認識 `ShellController`；工具不認識 `ImageCanvas`（只認識 `IToolHost`） | `tst_tools` 的 `missingControllersAreTolerated`；`check-deps` 在 S03 之後才有 |

## 決策審查（engineering-workflow 第 3 節）

| 面向 | Decision | Evidence | Verification |
|---|---|---|---|
| 架構與介面 | 見上方 SOLID；`ImageView` 放進 `domain`（最小版：指標、寬高、stride、格式、共享擁有者）；所有權規則 S06 定案 | `src/domain/image_view.h` 只用標準庫；`ICanvasView::showImage` 需要一個不是 `QImage` 的型別 | `tst_canvas::imageViewIsWrappedWithoutCopying` |
| 資料契約與生命週期 | 場景座標＝影像像素；`ImageView` 的 `owner` 讓緩衝活到最後一個 `QImage` 消失；快照用 `shared_ptr<const>` 交給畫布 | `ImageItem::wrap` 把 `owner` 放進 `QImage` 的清理函式 | `wrappedBufferLivesAsLongAsAnyQImageUsesIt`、`canvasHoldsTheBufferWhileItShowsIt` |
| 所有權與資源釋放 | 畫布不擁有 controller（原始指標，可為空）；`MainWindow` 解構前先把畫布的 controller 設為空 | 工具與畫布都容忍空的 controller | `missingControllersAreTolerated` |
| 並行、順序與狀態 | S02 沒有執行緒；所有 UI 在主執行緒。Qt 6.8 的 `LanguageChange` 排入佇列、`QGraphicsScene` 的更新通知也排入佇列，量測與測試都照事件迴圈的順序處理 | 實測（`perf.md` 的方法）與 [ADR-007](../../adr/007-copy-json-as-translation-source.md) | 測試用 `processEvents`／`sendPostedEvents` |
| 可靠性與錯誤處理 | 影像解不開：訊息列顯示錯誤卡片，舊影像保留；沒有影像時所有指標操作被忽略 | `ImageLoader` 回傳 `error` | `aBrokenFileShowsAnErrorCardAndKeepsTheOldImage`、`noImageMeansNoImageInteractions` |
| 安全與信任邊界 | 唯一的外部輸入是影像檔與拖放的路徑；解碼交給 Qt 的 `QImageReader`（上限 1 GB 的配置）；不執行、不轉送任何內容 | `loadImageFile` | `droppingANonImageFileIsRefused`（只接受影像副檔名）。**未做**：惡意影像檔的模糊測試 |
| 效能與容量 | 整數像素的填色細矩形（`BoxFrames`）＋層級簡化（LOD）＋只畫露出範圍＋批次層；虛線狀態仍用 cosmetic 描邊（乘 `devicePixelRatio`）。先寫簡單版、量測、依證據改（第一版描邊 I1 p95 約 50 ms） | `perf.md`：微基準、依縮放的分解、改前改後、逐一 vs 批次、真實視窗；DPR 實驗（DPR 2 時 cosmetic 寬 2 只有 2 個裝置像素） | `perf_ui_baseline`（Release 才把預算當失敗條件）；`tst_items` 的畫法、DPR、虛線測試；I1 超標部分見 ADR-009 |
| 交付、相容性與可觀測性 | CMake preset（debug／release／relwithdebinfo）、`/W4 /WX`；`AOI_WITH_OPENCV`、`AOI_WITH_HALCON` 預設 OFF；`ImageCanvas::framePainted` 信號與 `RoiLayerItem::lastPaintStats` 讓繪製成本可觀察 | 本機 MSVC 19.44 實際建置 | `scripts/dev/test.ps1`；`-DAOI_WITH_HALCON=ON` 在缺少標頭時明確失敗（見 `perf.md` 的環境驗證） |
| 測試與驗證 | Qt Test 5 個單元測試執行檔＋1 個效能基準線，offscreen 平台 | `tests/ui/` | 見 `perf.md` 與 `docs/current-status.md` |

## 與計畫的差異（誠實紀錄）

- **拖曳移動元件沒有做**（`ICanvasController::onComponentsMoved` 只有介面）：移動要有模型與復原才有意義，屬於 S04。
- **`ImageView` 提前放進 `domain`**：S03 計畫的第 1 項列了 `ImageView`，S02 的畫布需要零複製地顯示共享緩衝，所以先放最小版；S03／S06 可以擴充欄位，欄位意義不改。
- **訊息列固定高度**：與 S01 規格字面不同，見 ADR-008。
- **批次繪製的原始假設只有部分成立**：見上方「Why the layer draws the boxes this way」的更正與 `perf.md`。
- **框的粗細取整**：為了速度，框用整數像素的填色矩形畫，S01 的 0.5 px 差距（Pin ＝ 狀態寬度 − 0.5）在畫面上取整（Found：主體 2、Pin 1、窗 1）；虛線與點線仍用描邊。已記進 `ux-spec.md` 的修改紀錄。
- **I1 在預算邊緣、真實視窗超標**，預算數字沒改；選項與建議在 [ADR-009](../../adr/009-ui-toolkit-review-after-s02.md)，**待使用者決定**。
- **新增了開發環境腳本與一個建置環境的發現**：`scripts/dev/env.ps1`、`test.ps1`；繁中 Windows 下 `cl` 的 `/showIncludes` 前綴與主控台字碼頁的問題（詳見 `perf.md` 的環境驗證）。
- **沒有做的 UX 項目**：字級與主題的設定不會記住（沒有 `QSettings`）；忙碌轉圈與減少動態（沒有任何動畫）；方向鍵微調（S01 列為待決）；高 DPI 只驗證了畫筆寬度的換算，**沒有在真正的高 DPI 螢幕上看過**。
