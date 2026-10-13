# S11 — 效能優化（先量再改）：目的、原因、效果

> 規則：英文區塊用 ASD-STE100 風格（短句、主動語態、一詞一義）。繁中摘要放在每段之後。
> 檢查：`node tools/ste-lint.mjs`

<!--STE-->
## Purpose
This step makes the slowest measured part of the program faster. We change only the parts that the measurements show as slow. Each change has a number before and a number after on the same machine and the same day.
<!--/STE-->

**繁中**：只改實測證明慢的地方。S10 的報表裡只有一個指標超出預算：**U3**（分數滑桿變動到重繪，5 832 個元件，預算 16 ms，實測 p95 23–27 ms）。搜尋類與互動類的指標都在預算內（搜尋類最接近預算的是 HALCON 的 T4，p95 約 43 ms 對 60 ms），所以這一步只做 U3，分三個優化、三個 commit，每個都有同一天、同一台機器的 before／after。

<!--STE-->
## Reason
A guess about speed is often wrong. S04 said that the repaint of the canvas is the main cost of U3. It did not split the repaint. S11 first added two diagnostics. The image takes 1 ms of a 14 ms repaint. The box layer takes 12.5 ms. The box layer fills eight small rectangles for each of 5 832 boxes. The scene builder also formats a label and a row name for each object on each change. The canvas shows a label only when the user zooms in. With accepted components in the background, the scene builder rebuilds them on each change, but they do not change.
<!--/STE-->

**繁中**：先量再改。S04 只說「主因是整個畫布的重繪」，沒有把重繪拆開；S11 先在 `perf_ui_s04` 加兩個診斷（`U3-canvas-paint-image`、`-layer`），量到一次重繪 14.2 ms 裡影像 1.1 ms、**框層 12.5 ms**：5 832 個框每個填 8 個小矩形（外框 4 ＋ 描邊 4；點線更多），一個矩形約 0.28 µs。場景重建 3.5–5.5 ms：每個提案物件每次都格式化分數字串與「#編號」，但標籤只在放大到外框 ≥ 48 px 時才畫、列名只有看得到的幾十列會顯示。背景有已接受的元件時（U3-bg）再多 9 ms：門檻一動只有提案變，文件裡的元件卻每次重算（展開框、名稱、列）。

<!--STE-->
## Effect
Three changes, each with a test and a commit:

1. The box layer paints each small box as a cached glyph. One glyph per state, kind and screen size. One paint of the box layer goes from 12.5 ms to 1.15 ms. The pixels are the same as before.
2. The scene builder keeps the score as a number and formats the label only when the canvas paints it. A proposal row has no name string; the model builds "#id" on demand. The scene build goes from 3.5 ms to 0.9 ms.
3. The presenter keeps the document part of the scene in a cache. The cache key is the document revision and the label texts. With 4 883 accepted components in the background, U3 goes from 17.8 ms to 7.2 ms.

U3 is now inside its budget: median 6.5 to 8.8 ms, p95 7.6 to 11.2 ms. The budget file marks U3 as calibrated, and the performance test no longer lists it as a known item. I1 and I3 are faster too. The search metrics did not change.
<!--/STE-->

**繁中**：三個優化、三個 commit（數字都是 Release、offscreen、同一台機器、同一天）：

| # | 優化 | 改在哪 | before → after |
|---|---|---|---|
| 1 | 小框用字形畫（[ADR-024](../../adr/024-box-glyph-cache.md)）：每種（狀態、種類、螢幕寬高）畫一次成小圖，每個框一次 blit；像素和填色矩形完全相同（測試逐像素比對） | `RoiLayerItem`、`BoxFrames::snap` | 框層一次 paint 12.5 → 1.15 ms；U3 中位數 18.9 → 13.5 ms、p95 23.5 → 17.6 ms；I1 p95 8.1 → 4.5 ms；I3 22.9 → 7.1 ms |
| 2 | 標籤與列名延後格式化：`ComponentView` 存 `text` ＋ `score`，`label()` 畫的時候才組；提案列的名稱留空，`displayName()` 顯示時組「#編號」 | `SceneBuilder`、`ComponentSnapshot`、`ObjectRows`、`ObjectsModel` | 場景重建 3.46 → 0.89 ms；U3 13.5 → 8.75 ms、p95 17.6 → 11.2 ms（**進入預算**） |
| 3 | 文件那一部分的場景快取：`InspectionDocument::revision()`（每次變更通知加 1）＋ `DocumentSceneCache`（鍵 ＝ 修訂號 ＋ 標籤文字） | `InspectionDocument`、`SceneBuilder`、`AutoRoiPresenter` | U3-bg（背景 4 883 個已接受的元件）17.8 → 7.2 ms、p95 22.8 → 8.7 ms |

之後 `perf/budgets.json` 的 U3 標成 `calibrated`（limit 沒有改）、`perf_ui_s04` 的 `knownOver("U3")` 拿掉：從現在起 U3 超標會讓測試失敗、閘門也會擋。完整的數字、搜尋類沒有動的理由、基準線的重建在 [`perf.md`](perf.md)。

## 沒有做的（有數字、沒有改）

- **搜尋類**：S10 的矩陣裡沒有指標超出預算。最接近的是 HALCON 的 T4（200 個視窗的驗證，p95 42.9 ms 對 60 ms）與 OpenCV 的 ±180° 角度範圍（239 ms，T5 預算 300 ms；產品預設 ±5°）。HALCON 每次搜尋都要 `create_shape_model`（13–47 ms，隨樣本大小），是教新樣本的固有成本；流程 B 的樣本庫快取（ADR-006 §7.3）要等樣本庫的 UI 那一步。S11 的規則是「沒有超標的數字不動手」，所以都沒改，列在 `perf.md` 當下一步的候選。
- **AB-U3**（A/B 視窗改門檻之後重新比較，p95 48.8–63.4 ms）：沒有預算，是不是問題是使用者的決定；它的畫布和主畫布同一個 `RoiLayerItem`，第 1 個優化已經涵蓋它的框層。
- **dpr ≠ 1 的字形**：微基準顯示 2 倍縮放時字形仍快 2 倍，但像素的等價沒有驗證，所以高 DPI 照舊畫填色矩形。這台機器是 96 DPI（dpr 1）。
- **I1 真實視窗**：ADR-009 的「真實視窗超標」沒有重量（`perf.ps1 -Window`）；offscreen 的 I1 p95 從 8.1 ms 降到 4.5 ms。

## SOLID 對照

| 原則 | 本步驟如何落實 | 如何驗證 |
|---|---|---|
| S | 字形快取只在 `RoiLayerItem`（畫框的那個類別）；取整規則集中在 `BoxFrames::snap`；文件快取是 `SceneBuilder` 的一個可選參數，Presenter 只持有它 | `tst_items`、`tst_scene`：各自可以單獨測 |
| O | 加字形是「新路徑 ＋ 既有路徑當後備」，填色矩形與描邊的程式沒有改；`buildScene` 沒給快取時行為不變 | `glyphsGiveTheSamePixelsAsFilledRects`、`theDocumentPartIsReusedUntilTheDocumentOrTheLabelsChange` 的最後一段（有快取與沒快取的結果一樣） |
| L | `ComponentView::label()` 取代欄位，呼叫端（畫布、草稿框、測試）看到同樣的字串 | `tst_scene` 的標籤測試照舊通過 |
| I | `InspectionDocument` 只多一個 `revision()`；`DocumentSceneCache` 的欄位只有快取需要的 | `tst_document` 的 `theRevisionCountsEveryChangeAndNothingElse` |
| D | 快取的鍵來自 application 層的修訂號，presentation 不自己猜文件有沒有變 | 刪除、復原、換語言都讓快取失效（測試） |
