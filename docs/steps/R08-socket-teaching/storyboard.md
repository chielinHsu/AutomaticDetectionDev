> 目前主要流程已改為 [區域 CAD 註冊](region-workflow.md)，見 [ADR-036](../../adr/036-socket-region-cad.md)。下列為原版歷史。

# Socket 樣本流程分鏡

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| 2026-10-29 | Codex (gpt-6.1-sol) | 主流程改為 R 教樣本、F5 全圖搜尋、統一尺寸與樣本配方。 |
| 2026-10-28 | Codex (gpt-6.1-sol) | 初版亮塊定位與可編輯框。 |

| 時間 | 畫面 | STE 英文旁白 | 繁中字幕 |
|---|---|---|---|
| 0–5 秒 | 開啟圖片 | Open the source image. | 使用原始 RGB／灰階照片。 |
| 5–12 秒 | R 教左側、右側代表焊點 | Teach representative joint samples. | 每次只框一個焊點，涵蓋不同外觀。 |
| 12–18 秒 | F5，所有相似位置出現固定尺寸綠框 | Search the whole image. | 第一樣本決定新結果尺寸。 |
| 18–24 秒 | 看分數，調門檻後 F5 | Review the similarity scores. | 門檻提高可能減少誤框，也可能漏框。 |
| 24–32 秒 | V 改框、S 補框、Delete 刪除 | Edit the result boxes. | 人工確認結果。 |
| 32–40 秒 | 存 JSON，再開配方重搜 | Save the samples and recipe. | 原圖另外保存，還原後仍可搜尋。 |

本次只交付分鏡，未產生語音或影片。
