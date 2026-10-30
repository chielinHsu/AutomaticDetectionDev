# ADR-033 Socket 焊點教學獨立視窗

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| 2026-10-28 | Codex (gpt-6.1-sol) | 依使用者要求建立可執行的圈選、產框、編輯與配方 MVP。 |

- 日期：2026-10-28
- 決定者：使用者（功能範圍）；Codex (gpt-6.1-sol)（實作）
- 狀態：採用

2026-10-29：[ADR-034](034-socket-sample-search.md) 取代本決定的「亮塊定位作主要 UI」部分，改為 R 教樣本、F5 全圖搜尋與統一尺寸。既有資料格式、獨立框編輯、取消與背景儲存原則保留。

## 背景

ImageCanvas 已有縮放、圈選、拖移與改大小。JsonRecipeStore 已有版本化、原子儲存；主視窗的 Save 尚未接線。AutoRoiPresenter::finishRegionScan 目前直接回 LibraryEmpty。既有模板搜尋及 R07 晶格修正各有獨立契約，不應為了 Socket MVP 改變這些行為。

## 決定

- 新增 SolderTeachingSession，管理單框幾何與復原；presentation 新增 SolderTeachingWindow，重用 ImageCanvas；ISolderRegionDetector 由 OpenCV adapter 實作，app 組裝。
- 主程式提供 --socket；一般主視窗提供入口，啟動獨立行程，讓兩種流程的資料與生命期分開。
- 第一版定位亮／暗連通區，提供 Otsu／手動門檻、面積、外擴；不稱為模型推論或缺陷判定。無 OpenCV 時可手動編輯並明確提示。
- 每個框是一個獨立 ComponentType 與 ComponentInstance。修改單框不改其他框；自動定位不產生虛構的信心分數。
- 既有 JSON v3 格式不升版；Board.name 在此視窗保存原圖絕對路徑。原圖另存；缺圖時可重新指定同尺寸圖片。未校正時 umPerPixel 保留 0，依既有幾何慣例以 1 作內部換算，介面只顯示像素。
- 區域重掃只移除中心落在區域內的 Auto 框，保留區域外與手動框；零結果不修改。使用者仍需審核候選位置。
- 背景辨識使用 JobRunner 與 UiPoster，取消立即恢復操作並作廢舊世代。儲存使用 RecipeSaver，成功才標記儲存。

## 否決的替代方案

- 接到模板搜尋的 BoardProposal 並新增候選來源：會改變接受／替換全板等現有規則，擴大 R07 的回歸範圍。
- 以影片截圖訓練通用焊點模型：沒有原始訓練資料與標準答案，無法驗證泛化能力。
- 另寫圖片畫布與 JSON 格式：重複既有功能，增加座標與相容性問題。

## 後果

此視窗只編輯獨立、未旋轉的單 Body 框；其他形態的既有配方明確拒絕並保留原資料。定位參數是本次操作設定，v3 配方只保存框、原圖位置與既有資料欄位，不保存這組亮塊篩選參數。亮塊不能等同焊點語意，更不能等同焊接合格。後續可新增 detector adapter，而不修改視窗資料模型。
