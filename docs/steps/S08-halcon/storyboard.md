# S08 — 解說影片分鏡（60 秒）：兩種找法的差別

用途：讓使用者與新成員看懂 HALCON 的 shape model（看邊緣的方向）和 OpenCV 的 NCC（看灰階）差在哪裡、各自強在哪裡，以及「授權不對」時程式怎麼提醒。實際影片與語音合成是**選配**，需要使用者同意後才做；本檔是可直接交給剪輯或影片生成工具的腳本。
旁白為 STE 風格英文（每句 ≤ 20 字），字幕為繁中。畫面素材來自 [explainer.html](explainer.html) 的錄屏與 [diagram.md](diagram.md) 的圖。

主線：**同一個元件，兩種「像不像」的量法 → 光變暗、對比變低時，兩種分數都不變 → 反光、雜訊時兩種分數怎麼變（照示範頁實際算出來的）→ 分數高不等於位置準（實測）→ 合成板子上的實測並排 → 授權的提醒。**

**數字來源**：這台機器（i7-1360P）、Release 的實測（`perf.md`，三次執行的範圍）。畫面上要標出「這台機器、Release、合成影像」。

| # | 秒 | 畫面 | 旁白（STE 英文） | 繁中字幕 |
|---|---|---|---|---|
| 1 | 0–7 | 一個元件（主體＋兩端電極）的放大圖，左右兩半：左邊標「OpenCV：灰階」、右邊標「HALCON：邊緣的方向」 | The program can compare a part in two ways. OpenCV compares gray values. HALCON compares edge directions. | 比對一個元件有兩種方法：OpenCV 比灰階，HALCON 比邊緣的方向。 |
| 2 | 7–17 | 右半：沿著元件的邊緣出現一排小箭頭（梯度方向）；左半：灰階的格子 | The shape model keeps points on the edges. At each point, it keeps the direction of the edge. | Shape model 只留邊緣上的點，每點記住邊緣的方向。 |
| 3 | 17–27 | 說明頁的「亮度」滑桿往下拉：影像變暗；兩邊的分數條都幾乎不動 | When the light gets darker, both scores stay almost the same. | 光變暗時，兩種分數幾乎都不變。 |
| 4 | 27–37 | 「反光」滑桿拉到 10：電極上一塊白色亮斑；兩條分數都掉（示範頁上邊緣分數掉得比 NCC 多：亮斑的邊界多出了不屬於元件的邊緣） | A bright spot changes the gray values. It also adds false edges. Both scores drop. | 反光改了一塊灰階值，也多出不屬於元件的邊緣；兩種分數都掉。 |
| 5 | 37–45 | 「雜訊」滑桿拉到 30：兩條分數都還很高；畫面切到數字卡「強雜訊：HALCON 位置誤差最大 1.4 px，OpenCV 0.07 px」 | The scores stay high with noise. But a high score does not always give an exact position. | 雜訊大時分數仍高，但分數高不等於位置準。 |
| 6 | 45–53 | 數字卡並排：T2b 對 T2a（種子搜尋）、T4、召回率、位置誤差 p95（HALCON 比較大）、流程 B 每個類型的時間 | On these boards, HALCON finds all parts and is faster. OpenCV gives smaller position errors. | 在這些合成板子上，HALCON 全部找到而且比較快；OpenCV 的位置誤差比較小。 |
| 7 | 53–60 | 程式視窗：back end 選單的提示「HALCON 用的是 runtime 授權…SDK 的 dongle 沒有插上」；訊息列的吐司 | HALCON has two dongles. The program tells you when the wrong dongle is in use. | HALCON 有兩種 dongle；用錯的時候，程式會提醒你。 |

## 製作備註

- 鏡頭 3–5 直接錄 `explainer.html`：頁面用一個 32×16 的合成元件，即時算兩種分數（灰階的正規化相關、邊緣方向的平均一致度）。那是**示意的簡化版**（沒有金字塔、沒有次像素），不是 HALCON 的實作；畫面角落要標「示意」。
- 鏡頭 4、5 的「誰掉得多」照示範頁當下算出來的數字講（頁面的說明文字也是依計算結果產生的），不要寫死；示範頁是簡化版，只用來看分數的方向。鏡頭 5 的誤差數字取自 `perf.md` 的 T6（雜訊 12＋模糊 2 的板子）。
- 鏡頭 6 的數字以 `perf.md` 最後一次三次執行的範圍為準。**這台機器的速度會飄**，旁白不說數字，畫面上寫範圍。比較的結論（誰快、誰準）只對這組合成板子成立；真實影像的比較是 S09 的工作。
- 鏡頭 7 的提示文字取自 `docs/ux/copy.json` 的 `ui.backend.halcon.sdkDongleMissing`；dongle 的 ID 要遮掉或換成假的再出現在影片裡。
- 旁白速度約 140 字／分；每句不超過 20 字。
