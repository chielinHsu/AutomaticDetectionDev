# S00 — 解說影片分鏡（約 80 秒）

用途：給新成員與使用者快速理解「為什麼這樣設計」。實際影片與語音合成（例如 ElevenLabs）是**選配**，需要使用者同意後才做；本檔是可直接交給剪輯或影片生成工具的腳本。
旁白為 STE 風格英文（短句），字幕為繁中。畫面素材來自 [explainer.html](explainer.html) 的錄屏。

| # | 秒 | 畫面 | 旁白（STE 英文） | 繁中字幕 |
|---|---|---|---|---|
| 1 | 0–8 | 一排 40 支腳位，滑鼠逐一手畫框，計數器快速累加；再切到「拖一次→全部出現」 | A board has many pins. Drawing each box takes a long time. Now draw one box. | 一塊板子有很多腳位。逐一畫框很花時間。現在只畫一個框。 |
| 2 | 8–18 | 腳位陣列疊上均勻間距的格線，標示 pitch 與角度 | Parts in an array repeat. They have the same size, angle and pitch. We use this rule. | 陣列中的元件會重複，尺寸、角度、間距都一樣。我們利用這個規律。 |
| 3 | 18–40 | explainer 第 2 頁：六個階段逐一點亮（樣本→種子→擬合→預測→驗證→產生） | The program finds a few matches near the sample. It computes the pitch. It predicts where each part must be. It checks each place in a small window. | 先在樣本附近找幾個匹配，算出間距，預測每個元件的位置，再用小視窗逐一確認。 |
| 4 | 40–48 | 琥珀色虛線框浮現在一支「缺件」位置 | If a part is missing, the program still marks the place. The box is Inferred. The user checks it. | 如果某處缺件，程式仍會標出位置，狀態是「推算補上」，由使用者確認。 |
| 5 | 48–62 | 分層圖；OpenCV / HALCON 兩個方塊可切換，中間的 port 不動 | The program has two back ends. Both use the same ports. You can switch them. The rules above do not change. | 程式有兩個 back end，使用同一組介面，可以切換，上層規則不變。 |
| 6 | 62–74 | 效能表格，數字逐列點亮並出現綠色勾 | We do not trust a feeling of speed. We measure it. Each step has a budget. A slow change fails the test. | 我們不靠感覺判斷快慢，而是量測。每個步驟都有預算，變慢就會讓測試失敗。 |
| 7 | 74–80 | 步驟清單 S00→S12，S00 打勾 | We build the program in small steps. Each step ends with a commit. | 我們分小步驟完成，每一步結束都有一個 commit。 |

## 製作備註

- 鏡頭 3、4 直接錄 `explainer.html` 第 2 頁（拖動「缺件數」slider 到 2）。
- 色彩語言必須與 UI 一致：樣本＝藍色實線、找到＝綠色實線、推算＝琥珀色虛線、拒絕＝紅色點線。**不只靠顏色**，線型也要不同（色盲友善）。
- 旁白速度約 140 字/分；每句不超過 20 字。
