# S05 — 解說影片分鏡（75 秒）：為什麼找到幾個，就能推出全部

用途：讓使用者與新成員看懂「程式只找到一部分元件，為什麼能說出缺的在哪裡；什麼時候它會說『我不知道』」。實際影片與語音合成是**選配**，需要使用者同意後才做；本檔是可直接交給剪輯或影片生成工具的腳本。
旁白為 STE 風格英文（每句 ≤ 20 字），字幕為繁中。畫面素材來自 [explainer.html](explainer.html) 的錄屏（它的每一個結果都是 C++ 真的算出來的）。

主線：**陣列的元件間距相同 → 找到的元件之間的位移，最常出現的兩個就是格子的兩個方向 → 從中央長出整個格子，邊長邊修正 → 格子上沒有元件的格點，就是缺件 → 但錯的格子比沒有格子更糟：散佈的零件、間距不同的幾排、斜著穿過的一排，都不可以補出假的元件 → 量到的速度與正確率。**

**數字來源**：速度是這台機器、Release 的實測（`perf.md`，三次執行的範圍）；正確率來自 `tst_lattice_scenarios`（1 029 種組合，其中主張的範圍 420 種；總表在測試 log 與 `perf.md`）。畫面上要標出「這台機器、Release、合成資料」。

| # | 秒 | 畫面 | 旁白（STE 英文） | 繁中字幕 |
|---|---|---|---|---|
| 1 | 0–10 | 一塊板子上 60 個元件排成 10 × 6 的陣列；搜尋只找到其中 50 個（綠框），10 格是空的（看不出來哪裡少了） | A search finds most parts. It does not find all of them. Which parts are missing? | 搜尋找到大部分的元件，但不是全部。少了哪幾個？ |
| 2 | 10–22 | 從幾個綠框畫出到最近鄰居的箭頭；箭頭疊在一起，聚成兩叢（藍色的 u、v 兩個箭頭浮現） | The parts in an array have the same pitch. The most frequent steps between them are u and v. | 陣列的元件間距相同；元件之間最常出現的兩個位移，就是 u 和 v。 |
| 3 | 22–34 | 從中央的一個元件開始，格點一圈一圈往外長（範圍 6、12、24 格），每長一圈，u、v 箭頭微微修正 | The program grows the grid from the center. It corrects u and v at each step. | 程式從中央長出格子，每長一圈就修正一次 u、v。 |
| 4 | 34–44 | 格子長滿：沒有綠框的格點變成琥珀色虛線框（推算），旁邊角落顯示「真的缺件」的 × 與它們完全重疊 | Each empty node is a missing part. The program marks it for the user to check. | 沒有元件的格點就是缺件；程式把它標出來，讓人確認。 |
| 5 | 44–58 | 三個小畫面輪流：散佈的零件（沒有晶格）、三排間距不同（三組一排，沒有補）、一排斜著穿過另一排（兩組一排，沒有補）；每個畫面角落寫「安全的失敗」 | A wrong grid is worse than no grid. Scattered parts, mixed pitches and crossing rows give no false parts. | 錯的格子比沒有格子更糟：散佈的零件、間距不同的幾排、斜著穿過的一排，都不會補出假的元件。 |
| 6 | 58–68 | 說明頁的滑桿：雜訊、缺件拉高，狀態從「找到晶格」變成「信心偏低，建議檢查」，再變成「沒有晶格：只顯示找到的」 | With large noise or many missing parts, the program asks for a check. Then it stops guessing. | 雜訊大、缺件多時，程式先建議人工檢查，再不行就不猜。 |
| 7 | 68–75 | 長條圖（對數刻度）：1 000 點的擬合（中位數 0.56–0.85 ms、p95 0.65–1.19 ms，畫出預算 2 ms 的線）、約 4 900 點時舊的暫代版 97–134 ms、新的估計器 2.2–2.9 ms（中位數） | The program fits one thousand parts in about one millisecond. The budget is two milliseconds. | 一千個元件的擬合約一毫秒，預算兩毫秒。 |

## 製作備註

- 色彩語言與 UI 一致：樣本＝藍、找到＝綠（實線）、推算＝琥珀（虛線）、拒絕＝紅（點線）。不只靠顏色，線型也要不同。
- 鏡頭 2、3 的箭頭與「一圈一圈長」是示意動畫，**不是**程式的輸出；角落要寫「示意」。鏡頭 4、5、6 的板子畫面直接錄 `explainer.html`（資料是 C++ 算的，`tst_lattice_explainer` 匯出）。
- 鏡頭 5 的三個情況都在說明頁的「特殊情況」裡；錄屏時把「建議檢查」與「沒有晶格」的狀態列一起拍進去。
- 鏡頭 7 的數字以 `perf.md` 最後一次三次執行的範圍為準。**這台機器的速度會飄**（同一份程式差到 1.5 倍），旁白說「約一毫秒」，畫面上寫範圍，不要只放最好看的一次。
- 旁白速度約 140 字／分；每句不超過 20 字。
