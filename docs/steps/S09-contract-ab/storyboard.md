# S09 — 解說影片分鏡（75 秒）：兩個 back end 為什麼可以互換

用途：讓使用者與新成員看懂「同一個介面」不等於「同一個行為」、契約測試怎麼證明三個 back end 可以互換、測不出來的差異（分數不是同一把尺）怎麼在 A/B 視窗看見。實際影片與語音合成是**選配**，需要使用者同意後才做；本檔是可直接交給剪輯或影片生成工具的腳本。
旁白為 STE 風格英文（每句 ≤ 20 字），字幕為繁中。畫面素材來自程式的 A/B 視窗、[explainer.html](explainer.html) 的錄屏與 [diagram.md](diagram.md) 的圖。

主線：**一樣的介面、不一樣的行為 → 一組測試、每個 back end 一列 → 沒有授權時明確 SKIPPED → 故意弄壞，測試抓得到 → 測不出來的差異：分數的尺 → A/B 視窗：形狀與顏色、原因、門檻分開。**

**數字來源**：這台機器、Release 的實測（`perf.md`）。畫面上要標出「這台機器、Release、合成影像」。

| # | 秒 | 畫面 | 旁白（STE 英文） | 繁中字幕 |
|---|---|---|---|---|
| 1 | 0–8 | 三個方塊：fake、OpenCV、HALCON，都接在同一個插座（port）上；插座上寫「find(image, template, region, params)」 | Three back ends use the same interface. The compiler accepts all three. | 三個 back end 用同一個介面，編譯器全部接受。 |
| 2 | 8–16 | 同一塊板子三份：一份的框偏半個像素、一份的框轉錯方向、一份分數 1.3；紅色叉 | But the behavior can be different. The compiler cannot find a wrong sign or a wrong unit. | 但行為可以不同：正負號錯、單位錯，編譯器都看不出來。 |
| 3 | 16–26 | `diagram.md` 第 1 張圖動畫：registry 列出三個 back end → 每個測試函式長出三列 | One test suite runs on each back end. Each back end is one row in the log. | 一組契約測試跑每個 back end，log 裡一個 back end 一列。 |
| 4 | 26–34 | 測試的 log：HALCON 的列變黃，寫「SKIPPED: … error 2300: Dongle not attached」 | A back end without a license shows SKIPPED. The log tells the reason. A skip is not a pass. | 沒有授權的那一列是 SKIPPED，log 寫原因；跳過不是通過。 |
| 5 | 34–44 | 程式碼一行被改（OpenCV 的角度正負號）→ 三列變紅「angle max 6.2 deg」→ 改回來，變綠 | We broke the code on purpose. The suite failed on the correct back end. | 故意把程式弄壞：測試在被改的那個 back end 上失敗。 |
| 6 | 44–52 | 兩條分數條：OpenCV「灰階的相關」、HALCON「邊緣方向的一致度」；同一個 0.70 的刻度在兩條上的位置不同 | The two scores do not use the same scale. The same threshold does not mean the same thing. | 兩邊的分數不是同一把尺，同一個門檻意義不同。 |
| 7 | 52–63 | 程式的 A/B 視窗：綠圓（兩邊都找到）、藍方（只有 A）、橘三角（只有 B）；圖例勾掉綠的，只剩差異 | The A/B view shows the two results on one image. Each group has its own shape and color. | A/B 視窗把兩邊的結果疊在同一張影像上，每一類的形狀與顏色都不同。 |
| 8 | 63–70 | 點一個橘三角：右邊寫「OpenCV 的分數 0.6x 低於它的門檻 0.70」；把 A 的門檻往下拉，三角變成綠圓（不重新搜尋） | For each difference, the view tells why. A threshold change compares again without a new search. | 每個差異都說明原因；改門檻只重新比較，不重新搜尋。 |
| 9 | 70–75 | 數字卡：`perf.md` 的並排表（時間、召回率、位置誤差、峰值記憶體） | The numbers are in the step report. They are for this computer and these boards. | 數字在步驟報告裡，只對這台機器與這組板子成立。 |

## 製作備註

- 鏡頭 4 的 log 用 S09 開發途中實際的那一份（HALCON 讀不到 dongle，錯誤 2300；`perf.md` 有存檔的位置）；dongle 的 ID 要遮掉或換成假的再出現在影片裡。
- 鏡頭 5 的數字取自 `spec.md`「How we know that the suite can find errors」那一節的變異測試（OpenCV 角度正負號反過來：角度誤差 6.2–9.2°）。
- 鏡頭 7、8 錄程式的 A/B 視窗：主視窗教一個樣本 → 〔A/B 比較…〕。合成板子的情境選 `perf.md` 裡有差異的那一種（也可以錄 `explainer.html`：同一份資料、可以拉門檻）。橘三角的分數照當下畫面上的數字講，不要寫死。
- 鏡頭 9 的數字以 `perf.md` 為準。**這台機器的速度會飄**，旁白不說數字，畫面上寫範圍；結論只對這組合成板子成立，真實影像的比較見 ADR-004。
