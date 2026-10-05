# S03 — 解說影片分鏡（60 秒）：為什麼換 back end 不用改核心

用途：讓使用者與新成員看懂「核心為什麼不認識視覺函式庫」。實際影片與語音合成是**選配**，需要使用者同意後才做；本檔是可直接交給剪輯或影片生成工具的腳本。
旁白為 STE 風格英文（每句 ≤ 20 字），字幕為繁中。畫面素材來自 [explainer.html](explainer.html) 的錄屏。

主線：**use case 只認識 port → 換 back end ＝ 加一組 adapter（use case 一個字不改）→ 沒編進程式的 back end 明確失敗 → 有人偷 include 函式庫，依賴檢查擋下 → 新舊搜尋同時在跑，最新的贏 → 量到的成本如實列出。**

**數字來源**：所有數字都是本步驟在這台機器上的實測或測試的結果（機器指紋與方法見 [perf.md](perf.md)）。畫面上要同時標出「這台機器、Release、假 back end」。換機器數字會變，不要當成通則。

| # | 秒 | 畫面 | 旁白（STE 英文） | 繁中字幕 |
|---|---|---|---|---|
| 1 | 0–8 | 分層圖（`diagram.md` §1）：中間一塊「use case」，上面接畫面，下面接三個 port 插座；插座下方空著 | The use case does not know the screen. It does not know the vision library. It knows only ports. | use case 不認識畫面，也不認識視覺函式庫，它只認識 port。 |
| 2 | 8–20 | 插座上依序插上「Fake」「新的 back end（測試裡的 Scripted）」；每次插上，use case 方塊上的計數器都停在「改了 0 個檔案」；右下角列出新增的檔案：只有 adapter 與一行註冊 | A new back end is a new adapter and one line of registration. The use case changes by zero files. | 新的 back end ＝ 一組 adapter 加一行註冊，use case 一個檔案都不改。 |
| 3 | 20–28 | 插座上試著插 OpenCV 與 HALCON：彈出真實的錯誤訊息「…not built into this program yet. It arrives in step S07 / S08. Available back ends: fake.」，結束碼 2 | A back end that is not built fails at once. The message says why. It lists what we have. | 沒編進程式的 back end 馬上失敗，訊息說明原因，並列出可用的。 |
| 4 | 28–38 | 有人在 domain 的檔案加上 `#include <QString>`：依賴檢查器（終端機畫面）印出 `src/domain/bad.h:3 … domain must not include Qt`，結束碼 1，CTest 紅燈；右側列出 13 個 fixture 的綠勾 | A wrong include breaks the build. The checker names the file and the line. Thirteen test cases prove that it can fail. | 偷 include 會讓建置失敗，檢查器指出檔案與行號；13 個測試案例證明它真的會失敗。 |
| 5 | 38–50 | 時間軸：Job 1 跑到一半，使用者再框一次；Job 1 變灰（Cancelled），旗標升起；Job 2 完成並顯示結果；Job 1 的結果被丟棄 | The user starts a new search. The runner raises the old cancel flag. It does not wait. The screen drops the late result. | 使用者重新搜尋，執行器升起舊工作的取消旗標，不等它停下；畫面丟棄遲到的結果。 |
| 6 | 50–60 | 長條圖（實測，`perf.md`）：PerfProbe 關閉的成本在誤差內；開啟約兩百多奈秒；工作提交到開始約 0.04 毫秒。最後停在一行字：「簡化的晶格擬合在約 4 900 點要 100 ms 上下，S05 要換掉它」 | We measured the costs. A disabled probe costs nothing we can see. The simple grid fit is slow. Step S05 replaces it. | 我們量了成本：關閉的 probe 量不到差異；簡化版的晶格擬合很慢，S05 會換掉它。 |

## 製作備註

- 色彩語言與 UI 一致：樣本＝藍、找到＝綠、推算＝琥珀、拒絕＝紅；錯誤與違規用紅色。
- 鏡頭 2、5 的畫面是 `explainer.html` 的示意動畫，**不是**真的程式輸出；角落要寫「示意」。鏡頭 3、4 的文字是真的程式與檢查器的輸出（`explainer.html` 裡嵌入的是執行後逐字抄下的結果）。
- 鏡頭 2 的「0 個檔案」是 `tst_backend_registry::aBrandNewBackendRunsTheWholeUseCaseUnchanged` 的結構事實：這個測試只在測試檔裡新增 adapter 與註冊，`src/application`、`src/domain` 沒有任何變動。**不要**把它說成「任何 back end 都零成本」：真的 OpenCV／HALCON adapter 要寫，只是不必動 use case。
- 鏡頭 6 的數字以最後一次量測為準（`perf.md`）；「關閉的 probe 量不到差異」的意思是：差距在量測誤差內（約 ±1 ns），不是「精確等於 0」。
- 旁白速度約 140 字／分；每句不超過 20 字。
