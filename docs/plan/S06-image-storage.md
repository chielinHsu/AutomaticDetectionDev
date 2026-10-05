# S06 — 影像緩衝、轉接器、Recipe 儲存

| 項目 | 內容 |
|---|---|
| 層級 | 底層架構 |
| 分支 | `step/06-image-storage` |
| 依賴 | S03 |
| 產出位置 | `src/infrastructure/{image,storage}/`、`tests/support/synthetic_board.*`、`tests/infra/`、`docs/steps/S06-image-storage/` |

> **S01 修訂（2026-10-05）**：照 ADR-006：要加**樣本庫**儲存（建議 Project＋User 兩層、料號與別名索引）與板子（元件實例＋用到的類型）的儲存。使用者已決定：**樣本庫存 µm、影像帶校正值**；**板子保存自己用到的類型複本**（記得 `libId`），庫與板子只透過〔從樣本庫更新〕與〔存入樣本庫〕同步（ADR-006 §5、§6）。

> **S03 完成後的提醒（2026-10-05）**：S03 在標頭裡寫了 `IImageSource`、`IRecipeStore`、`ILibraryStore` 的契約（不存在回 `NotFound`；損壞的檔案回 `FormatInvalid`，不當機也不丟例外），以及 domain 型別 `Board`、`ComponentType`、`BoxGroup`、`Recipe`。`ImageView` 新增 `umPerPixel`（0 ＝ 未校正，既有欄位的意義沒變）。座標慣例（像素原點在左上角像素的左上角）見 ADR-010。

> **S04 完成後的提醒（2026-10-06）**：（1）`presentation/shell/ImageLoader` 與 `ImageViewAdapter` 是暫代；換成 `IImageSource` 時，`MainWindow` 的 `imageOpened`／`imageOpenFailed` 訊號與 `InspectionSession::imageOpened`／`imageFailed` 這兩條路不變。（2）影像沒有校正值時，`effectiveUmPerPixel`（`src/domain/box_expander.h`）暫以 1 µm ＝ 1 像素展開框：這是「暫定」，不是事實；`ImageView::umPerPixel` 有值就用校正值。（3）`InspectionDocument` 只在記憶體裡：`isDirty()` 與 `markSaved()` 已經就位（`UndoStack` 追蹤乾淨位置），存檔時呼叫 `markSaved()`；Presenter 還沒有「存檔」與「關閉前問是否儲存」。（4）接受提案時產生的元件類型是暫時的，沒有料號，名稱是 `X1…`；樣本庫 UI 與類型編輯器沒有做。

<!--STE-->
## Purpose
This step builds the image buffer, the converters and the recipe store. It also builds a generator for test images.

## Reason
A copy of a 5 MP image costs time and memory. A shared view costs almost nothing. A test image with known answers lets us measure accuracy.

## Effect
OpenCV, HALCON and Qt read the same pixels with no copy. We can test speed and accuracy on images that we can repeat.
<!--/STE-->

**繁中**：影像用「共享的 view」在 Qt、OpenCV、HALCON 之間零複製傳遞；加上可重現、帶標準答案的合成影像產生器，之後才能同時量速度與準確度。

## 任務

1. **所有權模型定案**：`ImageView`（資料指標、寬高、stride、像素格式）＋共享擁有者（`shared_ptr<void>` 或類似）。文件寫清楚生命週期規則。
2. **零複製轉換**：`cv::Mat` ⇄ `ImageView`、`HalconCpp::HImage` ⇄ `ImageView`（盡量用外部記憶體建構）、`QImage` ⇄ `ImageView`。凡是必須複製的格式，**明確標示並計數**。
3. **`IImageSource` 實作**：`OpenCvImageSource`（`cv::imread`，支援 16 bit/TIFF）、`HalconImageSource`（`read_image`）、Qt fallback（`QImageReader`）。各自受 `AOI_WITH_*` 選項控制。
4. **`SyntheticBoard` 產生器**（tests/support）：指定 pitch、傾斜、雜訊、模糊、缺件、反光；輸出影像＋ground truth JSON。
5. **Recipe 儲存**：`IRecipeStore` 的 JSON 實作（Qt Core）；含 `schemaVersion` 與遷移掛勾；非同步寫檔。
6. **測試**：零複製（指標相等）、**複製次數 = 0 的斷言**、Recipe 往返、v1 fixture 可被讀取、損壞檔案回傳錯誤而不是當機。

## SOLID 重點

- **S**：轉換、載入、儲存各自獨立。
- **L**：所有 `IImageSource` 對同一影像回傳相同 `ImageView` 語意（格式、座標）。
- **D**：Recipe 儲存只依賴 port 與 domain 型別。

## 效能檢查（寫入 `perf.md`）

5 MP／25 MP 影像載入時間；各轉換的複製次數（必須為 0，除非標示）；Recipe 存/讀 5 000 個物件的時間。

## 四種輸出（本步驟具體內容）

| # | 輸出 | 內容 |
|---|---|---|
| 1 | spec.md | 零複製與所有權的 Purpose/Reason/Effect |
| 2 | diagram.md | 影像資料所有權圖（誰持有、誰借用）；Recipe schema 圖 |
| 3 | explainer.html | 互動頁：選一種轉換路徑（Qt→OpenCV→HALCON），看「複製次數」與記憶體用量；調合成板子的參數預覽 |
| 4 | storyboard.md | 60 秒「為什麼 5 MP 影像不用複製」 |

## 完成定義

- [ ] 零複製與計數測試通過
- [ ] 合成板子可重現（同 seed 同輸出）
- [ ] Recipe 往返與壞檔測試通過
- [ ] 載入與儲存時間已量測
- [ ] `node tools/ste-lint.mjs --strict` 無 error

## Commit 範本

```
feat(infra): add zero-copy image adapters, synthetic board generator and recipe store

- ImageView ownership model; cv::Mat / HImage / QImage conversions with copy counting
- OpenCV, HALCON and Qt image sources behind AOI_WITH_* options
- SyntheticBoard generator with ground truth
- Versioned JSON recipe store
```

## Start prompt（開新 session 時貼上）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續 AOI 專案。請先讀 AGENTS.md、docs/current-status.md、docs/plan/S06-image-storage.md，
再依 .agents/harness/project-rules.md 的 Step Protocol 完成步驟 S06（分支 step/06-image-storage）。只做 S06。
完成後：commit、合併到 main、更新 docs/current-status.md 與交接單（ai-handoff skill）、對我提示 commit 內容，並用 spawn_task 為 S07 開新 session。
```
