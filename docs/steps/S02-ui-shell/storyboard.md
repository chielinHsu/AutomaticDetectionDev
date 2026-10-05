# S02 — 解說影片分鏡（45 秒）：5 000 個框是怎麼畫得動的

用途：讓使用者與新成員看懂「畫布為什麼這樣畫」。實際影片與語音合成是**選配**，需要使用者同意後才做；本檔是可直接交給剪輯或影片生成工具的腳本。
旁白為 STE 風格英文（每句 ≤ 20 字），字幕為繁中。畫面素材來自 [explainer.html](explainer.html) 的錄屏，以及 `tst_shell` 產生的截圖（設定 `AOI_SCREENSHOT_DIR` 後執行，見 [perf.md](perf.md) 的「重現方式」）。

主線：**第一版的畫法很慢 → 改成四條填色細矩形（快約 4 倍）→ 螢幕上太小的框乾脆不畫 → 批次層讓框很多時第一幀快很多 → 結果：五個預算通過、一個在邊緣、真實視窗超標，我們如實回報。**

**數字來源**：所有毫秒數都是本步驟在這台機器上的實測（機器指紋與方法見 [perf.md](perf.md)）。畫面上要同時標出「這台機器、Release、offscreen 或真實視窗」。換機器數字會變，不要當成通則。

| # | 秒 | 畫面 | 旁白（STE 英文） | 繁中字幕 |
|---|---|---|---|---|
| 1 | 0–6 | 深色板子上有 5 000 個框，使用者滾輪縮放、中鍵平移；角落小字：「這台機器 · Release」 | An AOI board can hold five thousand boxes. We measure how fast the canvas draws them. | 一塊板子可以有 5 000 個框。我們量畫布畫它們有多快。 |
| 2 | 6–16 | 並排兩個示意圖：左，每個框一條描邊路徑；右，每個框四條填色細矩形。下方兩根長條：18–22 ms 對 4.5–4.9 ms（2 600 個框，純 QPainter 微基準） | The first design strokes each box. It costs eight microseconds for each box. Four thin filled rectangles cost two microseconds. | 第一版用描邊畫每個框，每個框約 8 微秒；四條填色細矩形只要約 2 微秒。 |
| 3 | 16–25 | 縮放滑桿由 0.2 拉到 1.2：0.2 時只剩 160 個主體框；0.35 起 Pin 與窗出現；螢幕上標出「3 px」的門檻線 | A box smaller than three pixels is only noise. We skip it. The body box always stays. | 螢幕上小於 3 像素的框只是雜訊，不畫；主體框永遠保留。 |
| 4 | 25–33 | 長條圖：每個框一個 Item 對一個批次層。第一幀：5 000 框 34 對 16 ms；20 000 框 154 對 45 ms。標註：「平移縮放幀兩者相近」 | One layer starts faster than many items. With twenty thousand boxes, the first frame takes 45 milliseconds, not 154. | 框很多時，一個批次層的第一幀比很多個 Item 快得多：20 000 框是 45 毫秒，不是 154。 |
| 5 | 33–41 | 預算長條圖：U1a、U1b、U4、I2、I5 都在線下；I1 在線附近晃動（14–21 ms）；真實視窗那一根（22 ms）越過 16.7 ms 的線，標成琥珀色 | Five budgets pass. One budget sits at its limit. In a real window it is over. We report this. | 五個預算通過，一個在邊緣，在真實視窗裡超標。我們如實回報。 |
| 6 | 41–45 | 拖出藍色樣本框，畫布下方的訊息列出現「已框出區域」，影像上沒有任何文字；停在「ADR-009：待使用者決定」 | Messages stay under the image. The user decides what comes next. | 訊息留在影像下方。下一步怎麼走，由使用者決定。 |

## 製作備註

- 色彩語言與 UI 一致：樣本＝藍色實線粗、找到＝綠色實線、推算＝琥珀色虛線、拒絕＝紅色點線；疊圖配色固定亮色，兩個主題相同。
- 鏡頭 2、4 的示意圖是 `explainer.html` 的動畫，**不是**真的 Qt 畫面；長條上的毫秒數才是實測。製作時在畫面角落寫明「示意」。
- 鏡頭 3 的 3 px 門檻來自 S01 規格（`RoiLayerItem::kMinBoxScreenSide`），不是這一步發明的。
- 鏡頭 4 要同時秀出「平移縮放幀兩者相近」，否則會讓人以為批次是 5 000 框變順的主因（它不是，見 `perf.md`）。
- 鏡頭 5 的 I1 不要畫成單一數字：它在不同次執行落在 14.1–20.7 ms，真實視窗是 22.1 ms。
- 旁白速度約 140 字／分；每句不超過 20 字。
