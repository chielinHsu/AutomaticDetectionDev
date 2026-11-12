# R09 驗證與效能

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| 2026-10-30 | Codex (gpt-6.1-sol) | R09.18–R09.20：殘影、背景快取與輪廓草稿／完成交易。 |
| 2026-10-30 | Codex (gpt-6.1-sol) | R09.15–R09.17 的實作畫面、雙主題狀態與 qmake 聯合建置；完整 C++ 結果以最新 changelog 為準。 |

## 相依檢測框（R09.21–R09.23，2026-10-30）

- Python／Qt／ONNX 全套 59/59，0 failure、60.354s：`build/pcb-dl-env/Scripts/python.exe -m unittest discover -s tests/pcb_dl -v`，log `build/r09-inspection/regression-delivery.log`。新增 9 項，包含兩種真正 ONNX Runtime fixture 的整組輸出，並非正式品質模型。前輪全套 59/59、83.883s 在 regression-final.log；不以測試耗時變化宣稱最佳化。
- 獨立 payload build 成功：`python tools/pcb_dl/build_workbench.py --source . --output build/r09-inspection/payload`，28 檔；目前 Qt Creator 的 `AiInspTool/bin/vision-workbench` 原子更新 10 個程式檔，runtime.json 雜湊不變，全部 Python 來源逐檔一致。兩種 native `VisionCadWorkbench.exe --check --task obb|segment` exit 0；證據 payload-update.json、native-*.log。無需重編未改的 C++ launcher；未操作現有編輯視窗。
- 實際 5120×4096 z0 圖＋獨立 IC 定位示範，1 本體＋8 導腳均可見；dark／light／1366×768 預覽，未覆寫原圖或舊存檔，兩者 SHA 保持不變。`build/r09-inspection/preview.py`、preview.log、real-preview-and-perf.json、ic-related-*.png。這是起始幾何示範，未人工驗收框位，也不是正式模型預測。
- 純座標展開：5000 元件 × 9 框＝45000 區域，首次三次 1174.430／1163.181／1087.527ms（preview.log）；最終重畫後三次 619.129／421.965／594.108ms（preview-final.log／real-preview-and-perf.json），不同當下負載，不互相比較為效能提升。Python 3.12.14、CasparHsu1、Windows；不含影像推論、Qt 建 item 或繪製。新增能力沒有舊的等價基準；`perf/budgets.json` 沒有此指標，不設假預算／不稱 p95 或效能提升。
- 本輪 PARTIALLY VERIFIED：尚未重跑 C++／Rust 全套、GPU、真實正式模型、Pin 1／極性、產線 OK/NG，以及完整視窗高 DPI 人工驗收。R09.7 仍未完成。

## 前次殘影與輪廓互動的驗證紀錄

- 全套 Python／Qt／ONNX 50 tests、0 failures，78.754s；build/r09-drawing-all-final.log。未改 C++／Rust，未重跑其測試、GPU 或正式模型訓練。
- 回歸指令：python -m unittest tests.pcb_dl.test_canvas_repaint tests.pcb_dl.test_contour_interaction -v。繪製回歸會逐次讀 QWidget backingStore 的實際 QImage，與強制 viewport 重畫比較，沒有用會自動重畫的 QWidget.grab 掩蓋殘影。
- 修復前正常 DPI 的角點測試在 9／15／24% 留下 102／217／427 像素；只移除標籤把手仍失敗，補邊界後歸零。150% 的分段更新問題以 BoundingRectViewportUpdate 修正。
- 實圖與 seed=7 的 1024×1280 貼圖噪訊可額外重現背景取樣偏移（384 像素）；採 CacheBackground 與 drawBackground 後，相同操作與新圖／背景刷替換測試通過。只快取 viewport 背景，覆蓋層仍獨立更新，未採 FullViewportUpdate。
- 最後 5 項像素測試於 100%、150%、200% 顯示比例都成功；涵蓋 9／15／24／100／200% 畫布縮放、伸縮／取消／輪廓／背景貼圖。測試完整背板尺寸依 Qt 的半入取整換算，保留 0 像素差標準。
- 實際 5120×4096 舊 8 標註資料的獨立副本：3 次矩形草稿、3 次角點移動、1 次輪廓預覽，局部／完整重畫差均為 0；原存檔 SHA-256 不變。build/r09-drawing-real-check.py、r09-drawing-real-check.json、r09-drawing-real-final.log；草稿／完成畫面 r09-contour-draft-fixed.png、r09-contour-completed-fixed.png。示範新增只在該視窗記憶體，未保存至使用者資料。
- ready payload 建置成功、目前 Qt Creator 輸出與來源每個 Python 檔相同、OBB／segment native --check exit 0（r09-drawing-runtime-check.json）。直接 jom payload 曾因 runtime.json 被活躍父程序讀鎖而失敗；沒有把該次當成成功。最後保留等價執行設定、原子更新程式檔，沒有重鏈使用中的 exe，也沒有重啟現有視窗。
- 本步以像素正確性及操作交易驗證，沒有新增正式效能預算或宣稱 FPS／量產改善。DPR 自動測試不是整個應用程式的高 DPI 人工可用性驗收。


## Vision CAD Studio 實作（2026-10-30）

- 最新 Python／Qt／ONNX 全套：37 tests、0 failures，164.608s，包含背景縮圖、雙主題狀態保持、幾何交易與非 PCB 的 screw 類別。log：build/r09-workbench-final.log。首次 32 項有舊黃色像素斷言失敗，通用化文案後另有舊字串斷言失敗；兩者已更新為新行為並重跑，未隱藏失敗。
- 真實已存檔 5120×4096 圖／8 個原標註，在獨立 offscreen Qt 視窗渲染。1366×768 實際尺寸未擴張，清單 viewport 98px；右側可捲動，三欄可調寬。儲存檔 SHA-256 未變，未操作使用者現有未儲存視窗。證據 build/r09-workbench-dark.png、r09-workbench-light.png、r09-workbench-small.png、r09-workbench-ui-check.json、r09-verify-ui.py。
- 同一份視窗測 5 次切換主題：397.3–601.9ms、中位 475.2ms；5 次 refresh：37.7–88.3ms、中位 69.2ms。載入＋首次畫面約 13012.5ms。這是在 C++ Release／Debug 同時編譯時的本機觀察，包含 GUI 事件處理，沒有正式預算、不是 p95。
- 同步解碼全部縮圖的先前單次載入為 22762.0ms；程式確認會在 UI thread 讀取其他圖片。新增兩執行緒背景縮圖、沿用目前 pixmap 作縮圖，視窗銷毀／關閉取消剩餘工作。兩次量測沒有交錯 A/B 或控制系統負載，不能據此宣稱改善倍率。
- Workbench launcher Release／Debug／Profile 均編譯成功；兩種 task 各執行 --check，6/6 exit code 0，module 路徑均在對應 build/bin/vision-workbench。最終 payload 重建後須以 build/r09-launcher-checks.json 的最後紀錄為準。
- AOI_BUILD_PCB_WORKBENCH=OFF 搭配不存在的 Python 路徑 configure 成功且 Makefile 無工作台目標；Python -S 的缺依賴負向測試明確失敗且沒有發佈 runtime.json。一般 build 沒有 pip／下載。
- 完整 Release 最初因來源樹殘留 moc 失敗。測試 obj 只依賴 source moc，補上相同 shadow 目錄的明確 moc 相依後單例編譯成功；修正過絕對／相對路徑及 qmake contains 完整 regex 匹配。完整建置／C++ 測試的實際最終結果見本計畫 changelog，不以單例成功代替整套結果。
- 無正式模型重訓、GPU、真實品質或高 DPI 150%／200% 驗收。工作台 UI 改動不影響 C++ 演算法；未重新建立既有 C++ 效能預算。
- 最後的導覽高亮／空圖工具保護、平移點擊把手與字形修正後，5 項 shell 專項通過（5.395s）。三種 payload 再建置並逐檔比對 Python 源碼，6/6 native 啟動重跑通過。檢查腳本曾因重複 task 關鍵字建字典失敗，已修正腳本並重跑；不是產品啟動失敗。
- 最終 C++ Release：完整預設建置成功、67/67 回歸通過；8 組有 skip（缺真實資料／截圖環境、內建所有 back end 或格式寫入限制），2 個原有 XFAIL（OpenCV low contrast 角度、palette BMP）。HALCON backend 兩組實際通過，沒有把缺授權 skip 算成成功。
- 最終 C++ Debug：完整建置成功，66/67；tst_solder_teaching 的大量框拖曳門檻失敗。整組專項重跑 14 passed／2 failed／3 skipped。最後只跑拖曳函式：1017 框總拖曳 3201.38ms、release 2877.93ms（門檻 2000ms，失敗）；5000 框 1984.50ms、release 1891.05ms（通過），合計 3 passed／1 failed，exit 1。原因未判定，不宣稱整套 Debug 通過；Socket 來源與門檻未改。
- 工作台原生 launcher Release／Debug／Profile 都成功，Profile 本次只建 launcher／payload；沒有宣稱原 C++ Profile 全套已重建。
- 直接讀取 workbench_theme.COLORS 驗算 19 組實際 UI 色票／標註標籤對比全數通過，結果 build/r09-theme-contrast.json；只驗指定組合，不代表圖片所有像素或完整可及性驗收。

狀態：PARTIALLY VERIFIED；最新 16 項測試通過，實際 PCB 品質尚未驗證。

- 機器沿用本機 Windows 11／i7-1360P，推論預設 CPU。
- UI 以獨立 QThread 執行推論，tile 間檢查取消；訓練另用 QProcess，workers=0。
- tile 預設 1024、重疊 25%；最多 4096 tiles、20000 個合併前候選，配方最多 5000 CAD。
- 本計畫尚無既有 perf/budgets.json 指標；不自行宣稱達到量產延遲或準確度。
- 須記錄兩種 ONNX 管線測試、短訓練／test／匯出、Qt 操作與環境版本；合成資料只驗證接線。

## 實際結果（2026-10-29）

- Windows 11／i7-1360P／Python 3.12.14／Torch 2.15.0 CPU／PySide6 6.12.0／OpenCV Python 5.0／ONNX Runtime 1.31.1。
- 最終測試 8 passed，0 failure；上一個完整執行 48.111 秒，含首次載入模型及 Qt 操作，並非單圖推論延遲。最終以 build/r09-dl-tests-final-release.log 為準。
- 12 張合成影像（8 train、2 val、2 test，來源 group 各自獨立），兩種 YOLO11n 模式各 1 epoch、imgsz64、batch4；訓練／test／ONNX 匯出成功。
- 兩種 test mAP 均 0；只是流程 smoke，未宣稱可用 PCB 模型或正式準確度。固定 ONNX fixture 另外檢驗非空輸出的座標、旋轉、遮罩與 CAD 尺寸。
- 啟動結果在 build/r09-gui-smoke.json；套件版本及 pip check 已驗證。未跑 GPU 或量產負載。

## 真實圖接入

D:/BMP/PCB：12 張、5120×4096；9 個 R/G/B 通道逐像素核對相同。capture_import 新增 2 項測試通過。Qt 實際載入與渲染三張合併 RGB 分別約 1248.8／1032.4／885.7 ms，單次觀察，不是模型推論或 p95。標註數為 0、獨立來源 group 為 1，未做訓練或品質評估。證據 build/r09-real-pcb/。

## UX 與框線驗證

R09.9–R09.10：最終 16 tests passed／0 failed，最新時間見 log；新增 6 項新手操作、存檔取消及縮放下的框線像素驗證。框線 3px cosmetic＋6px 深色外描邊，選取 4px，頂點 10px。測量位置避開類別標籤，以免把標籤底框算進線寬。

真實 5120×4096 圖與八個標註，快取 pixmap 後 refresh＋processEvents 連續十次 17.4–32.1ms，中位 19.8ms；沒有相同條件的修改前基準，不宣稱改善倍率。未跑既有 perf harness、GPU 或模型準確度。證據 build/r09-ux-backup/verification.json 及 r09-ux-verified-final.log。

## 矩形與類別清單（2026-10-30）

全部 32 項 Python／Qt／ONNX 測試通過，47.354s；含六項形狀與五項類別查詢測試。實際 Qt 矩形拖曳預覽、轉換／Undo／存讀、收合狀態、精確 #編號搜尋、名稱、雙擊定位、群組及搜尋欄 Delete 保護均通過。

1000 個重疊合成標註的 UI 建立約 714ms；IC 30ms、電容 157ms、#999 19ms、名稱查詢 86ms，清除並重顯示全部約 666ms。這是本機單次合成序列，不是正式預算／p95，也不代表真實 PCB 辨識率。證據 build/r09-browser-perf.json。沒有重跑 C++／Rust 或正式訓練。
