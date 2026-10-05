# S04 — 解說影片分鏡（60 秒）：搜尋時畫面為什麼不會卡

用途：讓使用者與新成員看懂「搜尋在跑的時候，畫面為什麼還能動，按 Esc 為什麼馬上回來，按錯了為什麼能復原」。實際影片與語音合成是**選配**，需要使用者同意後才做；本檔是可直接交給剪輯或影片生成工具的腳本。
旁白為 STE 風格英文（每句 ≤ 20 字），字幕為繁中。畫面素材來自 [explainer.html](explainer.html) 的錄屏。

主線：**畫面只有一條執行緒，所以它不做搜尋 → 搜尋在另一條，結果以快照送回來 → 超過 0.2 秒才出現忙碌標示，超過 1 秒才出現進度與取消 → 取消時畫面馬上回到待命，遲到的結果被丟掉 → 結果出來是一份提案，接受是一個 Command → 接受錯了，一次復原回到提案 → 量到的數字如實列出（包含沒達標的）。**

**數字來源**：所有數字都是本步驟在這台機器上的實測（機器指紋與方法見 [perf.md](perf.md)）。畫面上要同時標出「這台機器、Release、假 back end、offscreen」。換機器數字會變，不要當成通則。

| # | 秒 | 畫面 | 旁白（STE 英文） | 繁中字幕 |
|---|---|---|---|---|
| 1 | 0–8 | 兩條泳道：上面「畫面執行緒」，下面「worker」。使用者在板子上框一個樣本；上面的泳道只有一小格「發出工作」，其餘是空的、持續轉動的游標 | The screen thread does not search. It starts a job and goes back to its work. | 畫面執行緒不做搜尋，它發出工作，就回去做自己的事。 |
| 2 | 8–20 | worker 泳道上有一條長長的進度；畫面泳道上，使用者同時在縮放板子、移動滑鼠，每一幀都畫出來。0.2 秒處亮起狀態列的「搜尋中…」；1 秒處訊息列長出進度條與〔取消 (Esc)〕 | After 0.2 seconds the status bar shows a busy label. After one second the message bar shows progress and a cancel button. | 超過 0.2 秒，狀態列顯示忙碌；超過 1 秒，訊息列出現進度與取消。 |
| 3 | 20–30 | 使用者按 Esc：畫面泳道上「回到待命」的方塊在 3 毫秒內出現（數字標籤：約 3 ms，預算 100 ms）；worker 泳道上的長條還在跑，變灰，最後送回的結果被一個「世代不符」的章蓋掉 | Escape returns the screen to idle at once. The worker keeps running. The screen drops its late result. | Esc 讓畫面立刻回到待命，worker 還在跑，遲到的結果被丟掉。 |
| 4 | 30–42 | 重新搜尋，結果回來：板子上出現綠、琥珀、紅的框；物件表同時列出同一批元件，點表格的一列，板子上對應的框亮起，反過來也一樣。滑桿拖動，綠框變紅（點線） | The result is a proposal. The table and the board show the same objects. The slider only filters the candidates. | 結果是一份提案；表格和板子顯示同一批物件；滑桿只是對候選重新過濾，不重新搜尋。 |
| 5 | 42–52 | 使用者按 Enter：提案變成板上的元件，訊息列說「已接受 91 個框 · Ctrl+Z 復原」。按 Ctrl+Z：框回到提案的樣子（虛線），再按 Ctrl+Y：又被接受。右邊疊一個小小的堆疊圖：兩個堆疊，一個標「提案」、一個標「文件」，箭頭顯示 Ctrl+Z 作用在哪一個 | Accept is one command. Undo returns the proposal with all its edits. One key press brings it back. | 接受是一個 Command；復原把提案連同它的修改一起還回來；一個鍵就回去。 |
| 6 | 52–60 | 長條圖（實測，`perf.md`）：物件表、搜尋期間的阻塞、取消、復原都在預算內；最後一根「分數門檻拖動」超出預算（p95 46–61 ms 對 16 ms），旁邊一行字：「重繪是瓶頸，下一步要優化」 | We measured every number. Four are inside the budget. One is not. We list it with its cause. | 每個數字都量了：四個在預算內，一個沒有，我們連同原因一起列出。 |

## 製作備註

- 色彩語言與 UI 一致：樣本＝藍、找到＝綠、推算＝琥珀（虛線）、拒絕＝紅（點線）；超標用紅色，達標用綠色。
- 鏡頭 1、2、3 的泳道動畫是 `explainer.html` 的示意，**不是**真的程式輸出；角落要寫「示意」。鏡頭 4、5 的板子畫面可以直接用 `tst_presenter::screenshotsForReview`（設定 `AOI_SCREENSHOT_DIR`）產生的真實截圖。
- 鏡頭 3 的「約 3 ms」（3 次執行的中位數 2.5–3.1 ms）是 `perf_ui_s04` 的 I4：從按下取消到 UI 回到 Idle 並重繪，**不含** worker 停下的時間（預算的定義就是這樣）。不要說成「搜尋在 3 毫秒內停止」。
- 鏡頭 6 的數字以最後一次量測為準（`perf.md`）；U3 在 5 832 個元件時中位數 37–46 ms、p95 46–61 ms；只有 504 個元件時中位數 11–16 ms、p95 14–24 ms，3 次執行裡有 2 次的 p95 也超過 16 ms：要照實說，不要只講好看的規模。
- 旁白速度約 140 字／分；每句不超過 20 字。
