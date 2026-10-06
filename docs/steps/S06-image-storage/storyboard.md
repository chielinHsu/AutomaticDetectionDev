# S06 — 解說影片分鏡（60 秒）：為什麼 5 MP 影像不用複製

用途：讓使用者與新成員看懂「同一張影像怎麼同時給畫面、搜尋、OpenCV 用，卻只有一份像素；什麼時候真的要複製，而且數得出來」。實際影片與語音合成是**選配**，需要使用者同意後才做；本檔是可直接交給剪輯或影片生成工具的腳本。
旁白為 STE 風格英文（每句 ≤ 20 字），字幕為繁中。畫面素材來自 [explainer.html](explainer.html) 的錄屏。

主線：**一張 5 MP 的彩色影像約 20 MB → 每個人各拿一份，記憶體與時間都翻倍 → 改成共用一份、每個人只拿「指標＋擁有者」→ 規則：交出去就不改，所以不用鎖 → 做不到的轉換一定複製一次，而且計數 → 量到的數字。**

**數字來源**：這台機器、Release 的實測（`perf.md`，三次執行的範圍）；複製次數來自測試的斷言（`tst_image_qt`、`tst_opencv_image`、`tst_shell`）。畫面上要標出「這台機器、Release、合成影像」。

| # | 秒 | 畫面 | 旁白（STE 英文） | 繁中字幕 |
|---|---|---|---|---|
| 1 | 0–8 | 一張 5 MP 的板子影像；旁邊三個方塊「畫面」「搜尋」「OpenCV」各自畫一份 20 MB 的長條，記憶體表累加到 60 MB | A 5 MP color image needs about 20 MB. Three copies need 60 MB and extra time. | 一張 5 MP 的彩色影像約 20 MB；複製三份就是 60 MB，還要時間。 |
| 2 | 8–20 | 三份長條合成一塊；三個方塊各拉一條線指到同一塊，線上標「指標＋擁有者」；擁有者旁邊的計數 1 → 4 | The program keeps one block of pixels. Each user holds a pointer and a shared owner. | 程式只留一塊像素；每個使用者拿到的是指標加上共用的擁有者。 |
| 3 | 20–30 | 畫面（UI 執行緒）與搜尋（worker）同時讀同一塊；一個「寫入」的手被擋下，寫著「交出去就不改」 | Nobody changes the pixels after the hand-over. So two threads read them without a lock. | 像素交出去之後沒有人再改，所以兩個執行緒可以不加鎖地同時讀。 |
| 4 | 30–42 | 說明頁的「轉換路徑」：選「PNG → Qt → 畫面」，複製計數維持 0；改選「PNG → OpenCV」，計數跳成 1，旁邊寫「BGR → BGRx，3 ms」 | Some conversions must copy. Each copy adds one to a counter, and a test checks the counter. | 有些轉換一定要複製；每複製一次，計數器加一，測試會檢查它。 |
| 5 | 42–52 | 長條圖：5 MP 載入（Qt）BMP、JPEG、PNG 的中位數；旁邊「複製：0」；OpenCV 的同一個 PNG 時間相近、「複製：1」 | Qt decodes the common files with no copy. So the program asks Qt first. | Qt 解碼常見格式不用複製，所以程式先問 Qt；Qt 讀不了的 TIFF 才交給 OpenCV。 |
| 6 | 52–60 | 配方檔：存到一半拔電的動畫，舊檔還在；數字「5 000 個元件：存 約 0.1 秒、讀 約 0.03 秒」 | A recipe save writes a temporary file first. A failed save keeps the old file. | 配方先寫暫存檔，成功才換名；存失敗時舊檔原封不動。 |

## 製作備註

- 鏡頭 2 的「計數 1 → 4」是 `shared_ptr` 的參照計數，示意用；角落寫「示意」。
- 鏡頭 4 直接錄 `explainer.html` 的轉換路徑選擇器；數字是頁內嵌的 `perf.md` 結果，不是頁面現場量的。
- 鏡頭 5 的數字以 `perf.md` 最後一次三次執行的範圍為準。**這台機器的速度會飄**（同一份程式差到 1.5–2 倍），旁白不說數字，畫面上寫範圍。
- 鏡頭 6 的「拔電」是示意；實際驗證是測試裡把目標檔設成唯讀，讓換名失敗（`tst_recipe_store`）。
- 旁白速度約 140 字／分；每句不超過 20 字。
