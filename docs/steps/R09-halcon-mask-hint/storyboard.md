# R09 — 解說影片分鏡（約 60 秒）

實際影片與語音合成是選配，需使用者同意。分鏡本身永遠要交付。
旁白：STE 風格英文（句子 ≤ 20 字）。字幕：繁中。

| # | 秒 | 畫面 | 旁白（STE 英文） | 繁中字幕 |
|---|---|---|---|---|
| 1 | 0–10 | 真實影像（LeftTopRegion）、HALCON、樣本是整個主體框；搜尋結果疊圖：長槽鋸齒上有綠框、陣列裡有缺 | With the whole body box, HALCON finds the slot teeth and misses pins. | 整框當範本：HALCON 把長槽的鋸齒當端子，陣列裡還有漏的 |
| 2 | 10–22 | 同一張，`M` 點端子的遮罩之後的疊圖：鋸齒沒有框、陣列幾乎滿 | A mask from one click on a pin removes the teeth. Recall goes from 80 to 97 percent. | 點一下端子的遮罩：鋸齒不見了，召回 80% → 97% |
| 3 | 22–38 | 程式畫面：S 框一個樣本 → 訊息列出現卡片「HALCON：建議用遮罩當比對區」，說明與兩個按鈕 | After you draw a sample, the card tells you why and what to do. | 建完樣本，卡片說原因、給做法 |
| 4 | 38–50 | 按〔用 M 點端子〕→ 樣本被選取、工具變 M → 點端子 → 選取資訊顯示「遮罩 n px」、卡片收起、提示回來 | The first button selects the sample and starts the M tool. One click sets the mask. The card closes. | 〔用 M 點端子〕→ 點一下 → 遮罩設好、卡片收起 |
| 5 | 50–60 | 切到 OpenCV：同樣的樣本，沒有卡片；按 F5 照樣搜尋 | OpenCV does not get the advice. The search is never blocked. | OpenCV 不提醒；搜尋從不被擋住 |

## 製作備註

- 色彩語言與 UI 一致：樣本＝藍色實線、找到＝綠色實線、推算＝琥珀色虛線、拒絕＝紅色點線。不只靠顏色，線型也要不同。
- 第 1、2 格的疊圖可以用 `tests/backend/perf_multi_sample_real` 產生（`AOI_MULTI_MATCH` 設矩形或 `mask:cx,cy,0`；R07 的 `status.md` 有用法）；真實影像不入版控。
