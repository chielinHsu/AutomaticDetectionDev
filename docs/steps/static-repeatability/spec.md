# 靜態重複性分析

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| 2026-10-16 | Codex (GPT-6.1-sol) | Excel 功能移植、動態指標、最多四組比較及指定 Box 折線圖 |

<!--STE-->
## Purpose
Calculate repeatability from repeated inspection reports.
## Reason
The workbook uses fixed metric names for some settings and charts.
## Effect
The tool reads numeric metrics and compares up to four folders.
<!--/STE-->

繁中摘要：以 CSV 直接計算各 Box 的靜態重複性。新指標透過欄位與設定擴充，不需要 Excel 或巨集。

## 操作

1. 從主視窗的〔檢視 › 靜態重複性分析…〕開啟獨立視窗。
2. 新增包含量測 CSV 的資料夾。每份 CSV 是一次量測，檔名以自然順序排列。
3. 勾選指標並選擇 Avg、Range、STDEV 或 P/T；在指標設定中調整輸出單位、倍率、SPEC 及門檻。
4. 只有一份資料夾時呈現全部勾選指標。新增第二至第四份後，每份以 ComboBox 選一張圖，同時並排比較。
5. 比較表使用共同完整 Box；改基準後顯示各組的差值。點圖表可查看該 Box 的原始量測及檔名。
6. 兩份以上可開啟〔跨資料夾折線圖…〕；橫軸是資料夾，以下拉選單選一個勾選指標，同時看 P/T 與 Range。
7. 指定 Box 留空時使用各組共同完整 Box 的最大 P/T、最大 Range；輸入 Box 名稱後只顯示該框。同名框跨 Component 時，以 Component/Box 指定。缺少或量測不完整的資料留空，不跨缺值連線；清空欄位恢復摘要。
8. 儲存分析會保存量測快照及指標定義。匯出會寫入兩份 CSV 及主分析視窗可見圖表的 PNG；獨立折線比較視窗目前不在此匯出範圍。

P/T 在 CSV 中保存為比值（`PT_ratio`），畫面顯示百分比。修改 SPEC 後所有資料夾重新計算；6 倍係數固定。

## 回歸範圍

- 計算：樣本標準差、空值與零、少於兩次、缺少 SPEC、不同來源單位。
- 匯入：引號、重複 Box、未知數值欄位、錯誤數值、檔名排序、取消。
- 比較：共同鍵、缺少指標、不同有效 Box 數、完整資料與部分資料分開。
- UI：一份全圖、兩至四份 ComboBox、上限、移除、勾選、載入失敗保留結果。
- 跨組折線：資料夾順序、共同完整母體、單一 Box 篩選、同名消歧、找不到／缺值與未設定 SPEC。
- 既有影像／搜尋／CAD 編輯不經過此分析流程。

實作決定與限制見 [ADR-029](../../adr/029-static-repeatability.md)。驗證結果見 [perf.md](perf.md) 及交接文件。
