# S11 — 解說影片分鏡（75 秒：我們怎麼讓它變快）

實際影片與語音合成是選配，需使用者同意。分鏡本身永遠要交付。
旁白：STE 風格英文（句子 ≤ 20 字）。字幕：繁中。

| # | 秒 | 畫面 | 旁白（STE 英文） | 繁中字幕 |
|---|---|---|---|---|
| 1 | 0–8 | 預覽畫面：5 832 個框，使用者拖分數滑桿，畫面跟著一頓一頓；右上角的計時 23 ms 對預算 16 ms（紅） | The user moves the score slider. The screen lags. The budget is 16 ms. The measured time is 23 ms. | 使用者拖分數滑桿，畫面會頓。預算 16 ms，量到 23 ms。 |
| 2 | 8–20 | `explainer.html` 第 1 張卡片：U3 的堆疊條，框層 12.5 ms 那一段最長、影像只有 1 ms | We measure before we change. The repaint takes 14 ms. The image takes 1 ms. The boxes take 12.5 ms. The scene build takes 4 ms. | 先量再改。重繪 14 ms：影像 1 ms，框 12.5 ms，場景重建 4 ms。 |
| 3 | 20–36 | `diagram.md` 第 3 圖動畫：一個桶的框 → 一張小圖 → 貼 5 832 次；旁邊微基準的兩個數字 5.13 → 0.35 ms | Each box was eight filled rectangles. Now we paint one small image per box size, and copy it for each box. The pixels are the same. One paint of the boxes goes from 12.5 ms to 1 ms. | 以前每個框填 8 個矩形。現在每種大小只畫一張小圖，每個框貼一次。像素一模一樣。框層從 12.5 ms 變 1 ms。 |
| 4 | 36–48 | 程式碼對照：`view.label = QString::number(score)` 劃掉 → `view.score = score`；標籤只在放大時出現 | The scene builder formatted a label for each object on each change. The canvas shows labels only when you zoom in. Now it formats a label only when it paints one. | 場景每次都替每個物件格式化標籤，但標籤只在放大時才畫。現在畫的時候才格式化。 |
| 5 | 48–60 | 畫面：板上已有 4 883 個已接受的元件，再預覽新的提案、拖門檻；`DocumentSceneCache` 的方塊亮起「命中」，計時 17.8 → 7.2 ms | With accepted components in the background, each change rebuilt them. Now the presenter keeps that part in a cache. The cache key is the document revision. | 背景有已接受的元件時，每次都重算它們。現在 Presenter 把那一部分快取起來，鍵是文件的修訂號。 |
| 6 | 60–75 | `perf.md` 的表：U3 四個階段 18.9 → 13.5 → 8.8 → 6.5 ms，預算線 16；`perf.ps1` 的 PASS；`budgets.json` 的 U3 變成 calibrated | Three changes, three commits, each with a number before and after. U3 is now 7 ms against a budget of 16. The gate enforces the budget from now on. | 三個優化、三個 commit，每個都有前後的數字。U3 現在 7 ms，預算 16。從現在起閘門會擋它。 |

## 製作備註

- 色彩語言與 UI 一致：樣本＝藍色實線、找到＝綠色實線、推算＝琥珀色虛線、拒絕＝紅色點線。不只靠顏色，線型也要不同。
- 第 2、6 段的數字取自 [`perf.md`](perf.md)（同一台機器、同一天；機器本身會飄約 2 ms，旁白用的是中位數）。
- 第 3 段強調「像素一模一樣」時，畫面可以放 `tst_items` 的 `glyphsGiveTheSamePixelsAsFilledRects` 通過的那一行。
- 第 5 段的計時是 U3-bg（診斷指標，沒有預算），要在畫面上標明。
