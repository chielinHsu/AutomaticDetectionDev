# S07 — 解說影片分鏡（75 秒）：由粗到細，為什麼先看小圖

用途：讓使用者與新成員看懂 OpenCV back end 怎麼在一張 5 MP 的影像上，用不到 0.1 秒找到近千個元件，而且位置準到十分之一個像素。實際影片與語音合成是**選配**，需要使用者同意後才做；本檔是可直接交給剪輯或影片生成工具的腳本。
旁白為 STE 風格英文（每句 ≤ 20 字），字幕為繁中。畫面素材來自 [explainer.html](explainer.html) 的錄屏與 [diagram.md](diagram.md) 的圖。

主線：**在大圖上每個位置、每個角度都比一次太慢 → 先在縮小 4 倍的圖上比（便宜 16 倍）→ 只在候選附近往下一層一層對準 → 最後一層用拋物線內插到次像素 → 角度範圍越大越慢 → 量到的數字與已知的限制。**

**數字來源**：這台機器（i7-1360P）、Release 的實測（`perf.md`，三次執行的範圍）。畫面上要標出「這台機器、Release、合成影像」。

| # | 秒 | 畫面 | 旁白（STE 英文） | 繁中字幕 |
|---|---|---|---|---|
| 1 | 0–8 | 一張 5 MP 的合成板子，一個 64×32 的樣本框；一個小方塊從左上角開始逐像素滑動，計數器跳到「500 萬個位置 × 3 個角度」 | The program compares the sample at each position and each angle. On a full image, that is slow. | 樣本要在每個位置、每個角度比一次；在原尺寸的影像上做，很慢。 |
| 2 | 8–20 | 影像縮成 1/2、1/4（金字塔），樣本跟著縮成 16×8；滑動的方塊在小圖上跑完，計數器「31 萬個位置」 | First, the program makes the image and the sample four times smaller. There are sixteen times fewer positions. | 先把影像和樣本都縮小 4 倍：位置少了 16 倍。 |
| 3 | 20–30 | 小圖上的分數圖（熱度圖）；亮點被圈起來，標「候選」 | High scores on the small image become candidates. Most of the image has no candidates. | 小圖上分數高的地方是候選；影像大部分的地方沒有候選。 |
| 4 | 30–42 | 一個候選放大：在下一層只比它附近 5×5 個位置、3 個角度；方框一層一層往下、越來越準 | At each larger level, the program checks only near each candidate. It moves toward the best score. | 每往下一層，只在候選附近比幾十個位置，往分數高的方向移。 |
| 5 | 42–52 | 第 0 層：三個分數點畫出一條拋物線，頂點落在兩個像素之間；標「0.1 像素」 | Three scores give a curve. The top of the curve gives the position between two pixels. | 三個分數畫出一條拋物線，頂點就是兩個像素之間的位置。 |
| 6 | 52–62 | 說明頁的角度滑桿：±5° → ±180°，時間曲線往上；頂層的角度數 3 → 57 | A wide angle range needs more angles on the small image. So it takes more time. | 角度範圍越大，小圖上要比的角度越多，時間越長。 |
| 7 | 62–70 | 數字卡：5 MP 種子搜尋（T2a）、200 個視窗的驗證（T4）、召回率 100%、位置誤差 p95 約 0.1 像素；旁邊預算 | The search meets the time and accuracy budgets on the synthetic boards. | 在合成板子上，搜尋的時間與準確度都在預算內。 |
| 8 | 70–75 | 真實影像（使用者的 12 bit 板子）縮圖：焊墊邊緣被框成黃色（0.70–0.85）；標「相似但不是」 | Similar shapes can get high scores too. A person must check the threshold. | 相似但不同的形狀也會得到不低的分數；門檻要人確認。 |

## 製作備註

- 鏡頭 1、2 的位置數是示意：5 MP 的影像在原尺寸約 500 萬個位置，縮小 4 倍後約 31 萬；實際的頂層角度數由範本大小決定（64×32、±5° 是 3 個）。
- 鏡頭 4 的「5×5 個位置、3 個角度」是精修的預設（± 2 像素、上一層的角度 ± 一步）；最好的點在邊上時會再移（爬坡），畫面可以示意一次移動。
- 鏡頭 6 直接錄 `explainer.html` 的角度範圍滑桿；曲線是頁內嵌的 `perf.md` 結果，不是頁面現場量的。
- 鏡頭 7、8 的數字以 `perf.md` 最後一次三次執行的範圍為準。**這台機器的速度會飄**，旁白不說數字，畫面上寫範圍。
- 鏡頭 8 用的是使用者提供的真實影像的疊圖（`perf_opencv_real` 產生）；**影像不入版控**，做影片前要再問使用者能不能出現在影片裡。
- 旁白速度約 140 字／分；每句不超過 20 字。
