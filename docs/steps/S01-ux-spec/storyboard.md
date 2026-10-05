# S01 — 解說影片分鏡（60 秒）：教一次，之後框一下

用途：讓使用者與新成員看懂「使用者怎麼用」。實際影片與語音合成是**選配**，需要使用者同意後才做；本檔是可直接交給剪輯或影片生成工具的腳本。
旁白為 STE 風格英文（每句 ≤ 20 字），字幕為繁中。畫面素材來自 [explainer.html](explainer.html) 的錄屏（桌面寬度，深色與淺色各錄一次更好）。

主線：**教一個新元件 → 存進樣本庫 → 下次框一塊區域就得到全部框 → 改一次類型，所有同類型元件一起更新。**

| # | 秒 | 畫面 | 旁白（STE 英文） | 繁中字幕 |
|---|---|---|---|---|
| 1 | 0–7 | 板上有 3 顆 IC，每顆 20 支腳，滑鼠逐一手畫 60 個框，計數器快速累加後停住 | A board has many parts. Each IC has twenty pins. Drawing every box takes a long time. | 一塊板子有很多元件，每顆 IC 有 20 支腳，逐一畫框很花時間。 |
| 2 | 7–19 | **教一次**：按 S，在一顆電容上拖出藍色粗框；訊息列出現進度；結果出現綠色實線；按〔接受並存入樣本庫…〕，輸入料號，按〔存入樣本庫〕 | Teach it once. Draw a box around one part. Save it to the library. | 教一次：在一個元件上拖出一個框，存進樣本庫。 |
| 3 | 19–29 | 類型編輯器：畫 Pin 框，列數改 2，按〔一次產生標準檢測框〕；Pin 與窗一次出現 | Set the rows. The program makes all the pin boxes at once. | 設好列數，程式一次建立所有的 Pin 框。 |
| 4 | 29–41 | **框一下**：按 R，在三顆 IC 周圍拖出一塊區域；選取其中一顆，右側列出候選樣本與分數；畫面上 3 顆 IC 各有 41 個框 | Now drag any region. The program matches it with the library. It lists every part it knows, with all of its boxes. | 之後只要框一塊區域，程式用樣本庫辨識，列出每個認得的元件和它的全部框。 |
| 5 | 41–48 | 區域裡有 3 顆電容沒有樣本：訊息列出現「教一個新樣本」；**畫面上的料件完全沒被擋住** | A part with no sample gets a hint. The message stays under the image. You see every part. | 沒有樣本的元件會有提示。訊息在影像下方，所有料件都看得到。 |
| 6 | 48–55 | **改一次**：雙擊一顆 IC，把 Pin 欄數改成 12；窗跟著變；儲存；3 顆 IC 一起更新 | Edit the type once. Every part of that type updates. | 改一次類型，同類型的元件全部一起更新。 |
| 7 | 55–60 | 按 Enter 接受；按〔匯出〕，彈出 CAD 清單（名稱、X、Y、角度、類型）；按 Ctrl+Z 回到預覽；停在狀態列「OpenCV · 99 ms · 找到 3 · 123 個框」 | Press Enter to accept. Export the CAD list. You can always go back. | 按 Enter 接受，匯出 CAD 清單。你隨時可以反悔。 |

## 製作備註

- 色彩語言與 UI 一致：樣本＝藍色實線粗、找到＝綠色實線、推算＝琥珀色虛線、拒絕＝紅色點線。**不只靠顏色**，線型也要不同。
- 鏡頭 2、4 把「模擬搜尋耗時」滑桿調到 2 400 ms 錄，才拍得到忙碌標示與進度條；狀態列的毫秒數是原型的模擬值，**不是實測**，影片中不要把它當效能數字。
- 鏡頭 5：用「快速跳到」→「預覽（區域掃描）」之外，再自己拖一塊含電容的區域，才會出現「教一個新樣本」。
- 鏡頭 1 的「60 個框」是手畫示意（3 顆 IC × 20 支腳），不是原型的功能。
- 旁白速度約 140 字／分；每句不超過 20 字。
