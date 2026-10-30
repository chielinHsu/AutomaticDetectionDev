> 目前主要流程已改為 [區域 CAD 註冊](region-workflow.md)，見 [ADR-036](../../adr/036-socket-region-cad.md)。下列為原版歷史。

<!--STE-->
## Current purpose
Users select a region without a sample. Structural rules locate horizontal Socket joints. Shared CAD profiles supply frame dimensions and inspection windows.
<!--/STE-->

繁中：R 選區不是範本，結構辨識後套共享規格；F5 註冊全圖。規則版僅支援 Socket 水平端點。

# Socket sample teaching

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| 2026-10-29 | Codex (gpt-6.1-sol) | 主流程改為 R 教樣本、F5 全圖搜尋、統一尺寸與樣本配方。 |
| 2026-10-28 | Codex (gpt-6.1-sol) | 初版亮塊定位與可編輯框。 |

## Purpose

<!--STE-->
The operator teaches one or more joint samples.
The tool searches the whole image for similar joints.
Every new result uses the width and height of the first sample.
<!--/STE-->

繁中：先教焊點樣本，再全圖比對；新結果採第一個樣本的尺寸，不沿每個亮塊外形改大小。

## Reason

<!--STE-->
Bright regions can have different shapes and sizes.
Sample matching uses the taught appearance and nearby image features.
A similarity threshold removes weak matches.
<!--/STE-->

繁中：區域亮塊可能包含焊墊或反光；改為模板外觀比對，門檻可調。沒有使用高度圖。

## Effect

<!--STE-->
Open the source image.
Press R and select one joint.
Teach other representative appearances if necessary.
Press F5 to search the whole image.
Review the similarity scores and boxes.
Use V to move or resize a box.
Use S to add a box manually.
Save the recipe and keep the source image.
<!--/STE-->

繁中：R 放開只建樣本；可連續教幾個，再 F5。V 編輯、Delete 刪除、S 補框、Ctrl+Z/Ctrl+Y 復原重做、Ctrl+S 儲存。

## 契約

- 最多 16 個樣本，每個只框一個焊點，長寬至少 8 px；樣本須有可比對的輪廓。左右外觀不同時各教一個。
- 門檻預設 90%，可調 50–99%；變更後按 F5 套用。提高通常降低弱匹配，也可能增加漏框。
- 多個樣本覆蓋不同明暗與外觀；跨樣本去重。先固定比例及角度，不以低分晶格推算補框。
- 所有新自動框採第一樣本大小，匹配位置使用主體中心；手動單框修改仍可獨立改尺寸。
- 樣本、結果、手動框於清單分開標示，結果顯示相似度。樣本是教學資料，不是合格標記。
- 重搜取代舊 Auto 結果，保留樣本與手動補框；空結果、錯誤或取消保留既有資料。
- 既有 JSON v3 保存 Sample 元件與搜尋門檻，開配方後可用外部原圖重新建立範本；不嵌入原圖。
- 輸入是 RGB／灰階照片，沿用 OpenCV 灰階模板比對，沒有高度圖需求，也沒有缺陷 OK/NG。
- 亮塊 API 僅保留相容驗證，舊亮度／面積控制不再顯示於主要流程。

## 啟動與驗證

用 scripts/dev/build-socket-mvp.ps1 建置，執行產物 AiInspTool.exe --socket；一般主視窗也有 Socket 教學入口。
完整操作見 [sample-workflow.md](sample-workflow.md)，架構決策見 [ADR-034](../../adr/034-socket-sample-search.md)。
新增測試涵蓋多樣本、尺寸統一、重複匹配、明亮干擾物、取消、滑鼠教樣本、F5、存讀及真實全圖。
沒有完整逐焊點標準答案前，不將匹配數宣稱為精確率或召回率。
