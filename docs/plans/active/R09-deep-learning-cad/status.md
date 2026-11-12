# R09 目前狀態

- 日期：2026-10-30
- 最新 R09.21–R09.23（本機未提交）：已建立元件定位 → 中央本體 → 導腳 → 相依檢測窗。第 3 步選參考標註，可用雙端／八腳起始框，或手動畫本體與任意導腳；樹清單選框，修改相對父框 X/Y/W/H/angle、刪子樹、Undo／Redo。同封裝同步，父框平移／旋轉帶動子框；尺寸不縮放子框。ADR-044。
- 本輪 PARTIALLY VERIFIED：59/59 Python／Qt／ONNX（60.354s），新增 9 項含兩模式真 ONNX Runtime fixture 產生 9 區域、滑鼠教框／資料匯出／存讀／越界拒絕／載模型保留規格；log build/r09-inspection/regression-delivery.log。check-deps 316 檔 0 違規、STE 1114 句 0 errors、結構檢查 OK（10 既有路徑警告）。初次結構檢查因 sandbox Git ownership 失敗，使用單次環境 safe.directory 重跑後 routing 68/68；未更改全域 Git 設定。
- 目前 Qt Creator 的 AiInspTool/bin/vision-workbench 已原子更新 10 個 Python 程式檔，全部來源一致，runtime.json 不變；獨立 payload build 28 檔成功，兩模式 native --check exit 0。既有視窗完全未操作，請使用者先完成／取消草稿並儲存，再重啟載入。沒有 commit／push、C++／Rust 來源未改；全套 C++／GPU／正式訓練與完整高 DPI 人工驗收本輪未跑。
- 實圖 5120×4096 的 IC 獨立示範已有 1 本體＋8 導腳，雙主題及 1366×768 已檢視，原圖與原標註 SHA 不變。示範專案 build/r09-inspection/ic-demo-with-inspection.json，截圖 ic-related-preview.png。這是人為起始幾何，未作真實框位品質驗收。5000 元件展開 45000 框約 422–1174ms，僅純座標運算／不同負載，不是 GUI 或產線預算。
- 相容：舊 v1 無 inspection 時保留原 window_ratio。新 profile.inspection 保存局部框圖，配方 inspection_boxes 含世界座標及親子 ID；重開由 pose/profile 重算。資料匯出只把元件當學習標註。長軸仍模 180°，未保證 Pin 1／極性，無新增焊接 OK/NG；R09.7 正式品質仍待人工資料。
- 最新 R09.18–R09.20：修復矩形／頂點殘影（5px 視窗邊界、BoundingRect 更新、靜態影像背景快取）；輪廓每點有標記、虛線預覽、點數與完成／取消。按完成、Enter／數字 Enter 或起點閉合後，驗證成功才建立 polygon 並切選取；錯誤留草稿。頂點視覺 12px、可點 18px，拖框把手跟隨。未完成顯示草稿，儲存／切資料／關閉須先完成或取消，不能誤顯示已存。
- 本次 PARTIALLY VERIFIED：Python／Qt／ONNX 50/50（78.754s），5 項畫布像素測試於 100%、150%、200% DPR 通過，實際 5120×4096／舊 8 框唯讀副本的矩形、拉角、輪廓對照均 0 像素差、原檔 hash 不變。見 build/r09-drawing-all-final.log、r09-repaint-final-dpi-*.log、r09-drawing-real-final.log。
- 已更新目前 Qt Creator 的 AiInspTool/bin/vision-workbench 程式檔，逐檔與來源一致，OBB／segment native --check 都通過。原先 jom workbench_payload 因現有啟動器持有 runtime.json（WinError 32）失敗；改在 build/r09-drawing-ready 成功建置，確認執行設定相同後只原子更新程式檔／bytecode，該設定原樣保留。既有編輯視窗完全未操作／重開，須使用者先完成或取消舊草稿、存檔後重啟載入新程式。
- 本次未提交／合併／push，未改 C++／Rust、模型／格式，未重跑 C++ 全套、GPU 或正式訓練；完整視窗的高 DPI 人工可用性未驗，DPR 結果只指畫布像素回歸。決策 ADR-043。
- Git：R09 功能提交 `abe86ef500b3` 已透過 `666d0d15d615` 非快轉合併至本機 main；本輪隔離暫存快照的工作台 5 項測試、qmake／jom 建置與兩種 task 啟動均通過。其他未提交差異及未追蹤路徑雜湊保持一致，未 push。
- 本次 R09.15–R09.17 已實作三欄 **Vision CAD Studio／元件深度學習 CAD**，不限 PCB；首次深色、可切淺色並記住偏好。圖片縮圖、類別／清單、選取名稱／類別／矩形幾何／裁切／幾何有效分工清楚；主題保留草稿、選取與縮放。對應 ADR-041，操作見 [README](../../../../tools/pcb_dl/README.md)。R09.14 兩張圖保留為歷史概念。
- Qt Creator 開 AiInspTool.pro 並 Run qmake／Build 後，執行組態可選 AiInspTool 或 VisionCadWorkbench；後者以 --task obb／segment 切換，可帶 --project。qmake 預設檢查及編譯 Python payload，不自動 pip／下載；AOI_PCB_PYTHON 可指定環境，AOI_BUILD_PCB_WORKBENCH=OFF 可只建原工具。
- 本次驗證 PARTIALLY VERIFIED：37 項 Python／Qt／ONNX 通過；最後修正後 5 項專項再通過。原程式與工作台 Release／Debug 建置成功，工作台 Profile 建置成功；6/6 native 啟動檢查及所有 Python 源碼與 payload 比對通過。Source moc 污染以明確 shadow 相依修正，沒有清理來源樹的舊產物。
- C++ Release 回歸 67/67（8 組有 skip、2 XFAIL）；Debug 66/67，原 Socket 的大量框拖曳門檻失敗。單獨整組重跑仍有 1017／5000 兩列失敗；最後僅執行拖曳函式，1017=3201.38ms 超過 2000ms、5000=1984.50ms 通過，整體 exit 1。原因未判定，沒有調高門檻或改 Socket 原始碼。證據 build/r09-cpp-*-regression.log、r09-cpp-debug-solder-recheck.log、r09-debug-drag-isolated.txt。
- 真實已存檔 20MP／8 標註的雙主題與 1366×768 已檢視，檔案雜湊保持不變；背景縮圖兩執行緒，關閉取消。實測見 [perf.md](../../../steps/R09-deep-learning-cad/perf.md)，非正式預算／p95。尚未驗高 DPI、正式模型品質；Profile 原 C++ 全套本次未重建。
- 狀態：雙模式工作台與標註／訓練管線已實作；PARTIALLY VERIFIED，真實 PCB 品質驗收待人工資料。
- 使用者確認：不限 PCB 的通用元件影像，可自訂類別／封裝；目前 PCB 例圖在 D:/BMP/PCB，正式品質驗收尚缺足夠人工資料。
- 入口：專案根目錄 StartCadTools.cmd，2=OBB、3=實例分割；1 保留 Socket 規則版。使用獨立 PySide6 工作視窗，不嵌入 C++ Qt 程序。
- 功能：矩形／多邊形人工標註、可拖頂點、整圖審核、group 分割、雙資料匯出／hash 檢查、各自訓練／test 評估／ONNX 匯出、manifest task／類別／SHA 驗證、ROI tiles、旋轉 NMS、固定類別 CAD 規格及覆寫、輪廓／子窗顯示、人工框保留、Undo/Redo、配方存讀。
- 驗證：目前全套 37 項 Python／Qt／ONNX 通過；最新全套 log：build/r09-workbench-final.log，最後專項 build/r09-shell-final.log。前次 32 項紀錄保留在 build/r09-browser-shapes-regression.log。
- 兩種 YOLO11n 模式各跑 12 張合成圖、1 epoch、imgsz64、CPU；訓練、獨立 test 評估、ONNX 匯出成功，重新匯入推論成功。合成 test mAP 都為 0，僅驗證接線，不能用來處理真實 PCB。model.json 標記 demo_only。
- 訓練證據：build/r09-train-obb-verified.log、r09-train-segment-verified.log；權重／manifest 在 build/r09-dl-training-verified/<task>/deployment。
- 操作：tools/pcb_dl/README.md。安裝環境 build/pcb-dl-env，requirements-lock.txt 鎖定本次環境，pip check 無破損依賴。
- 配方：pcb-dl-cad v1；標註：pcb-learning-project v1。與 Socket aoi-recipe v3 分開，需用本工作台開啟。照片保留本機絕對路徑。
- 未驗：使用者真實 PCB 精確率／召回率、正式 epochs 與資料量、GPU、量產延遲／機台。OBB 長軸不代表 Pin 1／極性；不做焊接 OK/NG。
- R09 已依本次授權本機提交與合併；R07 與另一聊天 R08.8 的未提交工作保留，未 push。

## 真實 PCB 圖片接入（R09.8）

- 來源 D:/BMP/PCB：z+1、z-1、z0-focus on pcb；共 9 張 8-bit 灰階 BMP 與 3 張 RGB PNG，全部 5120×4096。
- 每張合併 PNG 的 R/G/B 與 high-angle-red、middle-angle-green、low-angle-blue BMP 逐像素相等（9/9，MAE=0）。expsure.txt 記錄 red=3、green/blue=2。
- 視覺上為同板同視野不同 Z 拍攝，保守歸為單一來源群組 pcb-capture-001，不能分作獨立 train/val/test。
- 新增 tools/pcb_dl/capture_import.py，驗證通道／合併一致性、缺失檔案、禁止覆寫及原圖目錄內輸出；建立未審核／零標註專案。修正測試發現的缺少 R 通道時整組被略過問題。
- 新增 2 項匯入測試通過；原有 8 項未重跑（未更動其實作）。三張 RGB 在 Qt 工作台載入成功，單次載入與渲染約 886–1249 ms，非 p95。
- 起始專案 build/r09-real-pcb/annotation-start/project.json；audit.json、workbench-check.json、實際畫面及測試 log 在同一 scratch 根目錄。
- 尚無元件類別／輪廓標註與 CAD 規格，未以這批圖訓練或計算準確度。RGB 是不同角度照明的強度組合，不是每像素高度資料；不同 Z 不直接套相同標註座標。

## 新手 UX 與框線（R09.9–R09.10）

- 使用者回報：第一次使用不知道順序；縮小全圖只見 resistor 文字、框線近乎不可見。
- 介面改為五步任務頁；步驟提示、中文類別、工具選取狀態、主要動作、固定底部上一／下一步。進階訓練與辨識值預設收合；未具備資料／模型時顯示缺少條件並停用動作。
- 支援選取後修改標註類別、從標註帶入 CAD 尺寸；畫完顯示編號與新增回饋。小於 5 螢幕像素的意外拖曳提示先放大。
- 框線固定 3px，加 6px 深色外框；選取白色 4px、10px 頂點把手，中文標籤有深底與編號。標籤／子框隨主框拖動，圖像 pixmap 快取供重繪。
- Ctrl+S 儲存目前文件，Ctrl+Shift+S 另存標註；有未儲存內容時關閉可儲存／放棄／取消，取消存檔不得關閉。
- 最新 16 tests passed，0 failed；包含真實 ONNX OBB／分割、6 項 UX 操作與畫面像素驗證。log build/r09-ux-verified-final.log，執行時間見最終 log。
- 真實 20MP 圖與八框重繪 10 次：17.4–32.1ms，中位 19.8ms（僅本機單次序列，無先前基線，不稱量產 p95）。
- 原視窗八個標註透過 UI 兩次另存，保留原類別與幾何；持久檔案在聊天 outputs/PCB-UX-annotations.json，新版已載入並在桌面顯示。D 專案備份 build/r09-ux-backup/live-annotations-final.json。
- 新增 guided_ui.py、guidance.py、canvas_items.py、ui_theme.py 及 test_guidance.py；調整 window.py、workflow.py、canvas.py、cli.py；ADR-038，操作 docs/steps/R09-deep-learning-cad/ux-walkthrough.md。
- 本次只改 Python 工作台；未重跑 C++／Rust 套件或正式模型訓練。R09.7 真實 PCB 品質仍未驗，無使用者可用性研究，不宣稱所有新手都已驗收。
- 未 stage／commit／push，R07 與 Socket 原工作保留。

## Delete 鍵修復（R09.11，2026-10-30）

- 根因：原本只有刪除按鈕接 Workflow.delete，漏接 Delete 鍵；選取本身及步驟頁面不是主因。
- 在畫布與框清單掛 WidgetWithChildrenShortcut 的 Delete，重用既有刪除／Undo 交易。不使用全視窗 Delete，文字／數值欄位照常編輯；輪廓草稿未完成時不刪既有框。
- 補上 Delete 提示、刪除後編號與 Ctrl+Z 回饋；沒選取不變更資料。
- 新增 tests/pcb_dl/test_delete_key.py：原版 3 fail／1 pass；修復後全部 21 項通過，37.932s。證據 build/r09-delete-before.log、r09-delete-after.log、r09-delete-regression.log。
- 原視窗顯示未儲存調整，先透過 UI 儲存，最新 8 個框已保留；修改前與儲存後備份在 build/r09-delete-backup/。沒有在使用者的真實標註上試刪；以測試圖片驗證刪除。
- 只改 window.py、workflow.py、guided_ui.py、guidance.py 與新回歸測試。C++／Rust／模型訓練未重跑，原圖／R07 不動，不 stage／commit／push。

- 最終桌面驗證：已重開修正版本，顯示 Delete／Ctrl+Z 提示、8 個標註與已儲存狀態；未在使用者真實資料上試刪。結構檢查 OK（9 個既有路徑警告），依賴 313 檔 0 違規、STE 0 errors。

## 矩形與類別查詢（R09.12–R09.13，2026-10-30）

- 矩形以 shape=rectangle 保存意圖；角點拖曳維持直角與固定對角，支援旋轉／跨角，拖曳預覽與放開提交一致。多邊形須明確切換，舊資料只推定幾何類型、不改座標。轉回矩形採旋轉外包框，超界拒絕，可復原。
- 左側清單改為依類別收合的樹，顯示數量、搜尋中文／英文類別／名稱／#編號；搜尋同步篩選畫面，清除後恢復折疊狀態。單擊選取、雙擊／Enter 放大定位，可展開／收合全部或隱藏整個面板。
- 預設只顯示選取項的完整標籤；可勾全部標籤。名稱由使用者輸入，v1 可選 name 欄位，支援 Undo／存讀；#編號仍為目前排序，非永久 ID。
- 新增 annotation_geometry.py、object_browser.py、兩個測試檔，並更新 canvas／workflow／model／guidance／guided_ui／window／canvas_items；ADR-039。
- 32 項測試通過，47.354s；其中形狀 6 項、類別清單 5 項。log build/r09-browser-shapes-regression.log；原始失敗在 r09-shapes-before.log 與 r09-browser-tests.log。
- 1000 個重疊合成元件 UI 建立約 714ms；查詢 19–157ms，清除顯示全部約 666ms；非真實板與正式效能預算，不宣稱大量元件即時性能。
- 開啟已存檔的真實專案，確認原資料完全相同；畫面 build/r09-browser-preview.png、r09-browser-selected.png。
- 目前使用者視窗仍有新調整，工具偵測到使用者輸入後未成功送出儲存，沒有重開。build/r09-shapes-backup/ 只有舊已存檔 8 框，不能當最新資料覆蓋視窗。使用者須先 Ctrl+S 再重開以套用新版；不擅自重置目前進度。
- 原圖、C++ Socket／R07 與正式模型不動，本次未重跑 C++／Rust 或訓練，不 commit／push。
