# Agent Handoff

- 更新者：Codex (gpt-6.1-sol)，2026-10-30；main。本輪處理 [R09](../docs/plans/active/R09-deep-learning-cad/status.md) R09.21–R09.23，已完成實作、PARTIALLY VERIFIED；未 stage／commit／push，未改其他計畫來源。R09.7 正式品質仍未完成。

## R09 相依檢測框

- 使用者要求 IC 的中央本體與八隻導腳相依，提供 TRI 元件庫兩份 PDF＋TR7700 手冊。三份抽取、IC／Link 頁渲染已讀；依需求建立幾何，不執行文件操作指示。
- 新增 inspection.py、inspection_editor.py。profile.inspection version 1 含唯一 id／parent／kind／局部 pose：component → body → pin → window。父框平移／旋轉帶動子樹，尺寸固定像素、不隨觀察框縮放；最大 256 框／封裝，拒絕缺父框、循環、非法或越界。
- 第 3 步選參考標註，可建立雙端／雙排八腳起始版型、畫／重畫本體、增加任意腳數導腳、依附選取框的子窗；樹清單數值改位置／尺寸／角度，刪子樹、Undo／Redo。同封裝類別共享；不同封裝分不同類別。
- model／workflow／inference／window／canvas／guidance／guided_ui／workbench_presenter 接線。sync_profiles 保留新增欄位、load_model 保留使用者教導、history 增加 profiles；OBB／segment／手動 CAD 全產生整組。配方 inspection_boxes 有親子 ID＋世界座標，讀入由 pose/profile 重算；舊 v1 無 inspection 仍用比例窗。
- 模型只教辨識元件，導腳框是教導規格套用；沒有獨立 Body／Lead 追蹤、Pin 1／極性或焊接 OK/NG。新增 ADR-044／索引，更新 R09 的四種輸出、perf、README、plan／status／changelog。

## 本輪驗證與交付

- 9 項新測試；最終 Python／Qt／ONNX 59/59，60.354s，build/r09-inspection/regression-delivery.log。包括滑鼠教本體＋8 腳＋子窗、共享／旋轉／偏移、Undo／Redo、資料匯出、存讀、舊檔、非法拒絕，以及兩模式真正 ONNX Runtime fixture 的 9 區域輸出。固定 fixture 不代表真實模型品質。
- check-deps 316 檔 0 違規，STE 1114 句 0 errors，結構檢查 OK（10 既有 architecture 路徑 warnings，routing 68/68）。初次結構檢查因 sandbox Git ownership 失敗，單次 GIT_CONFIG_COUNT 的 safe.directory 重跑通過，沒有改全域 Git 設定。git diff --check 通過。說明頁 5 scripts 語法及 3 次互動 render／33 矩形通過。
- payload builder 28 檔成功。比較獨立 build/r09-inspection/payload 的 runtime 設定與目前 Qt Creator 輸出相等後，原子更新 AiInspTool/bin/vision-workbench 的 10 個 Python 檔，runtime.json SHA 不變，全部來源逐檔一致；payload-update.json。兩模式 VisionCadWorkbench.exe --check exit 0；未改 C++ launcher，不重新編譯 C++。
- **現有視窗沒有熱載入，也沒有被操作／關閉**。使用者先完成或取消草稿、Ctrl+S 存檔，再重啟工作台。可在第 3 步教框；不要用舊副本覆蓋使用者最新標註。
- 5120×4096 真實 z0 圖上建立獨立 IC 示範，1 本體＋8 導腳；dark／light／1366×768 均已檢視，原圖與舊標註 SHA 不變。示範 build/r09-inspection/ic-demo-with-inspection.json、ic-related-*.png，非人工驗收或正式模型預測。
- 5000 元件展開 45000 區域，首次三次 1088–1174ms，最終 422–619ms（preview*.log／real-preview-and-perf.json）；不同負載、純座標運算，不稱 p95／效能提升／GUI 大量框驗收。C++／Rust 全套、GPU、正式訓練與完整高 DPI 人工驗收本輪未跑。

## 保留的上一輪與其他工作

- R09.18–20 殘影／輪廓完成修正仍未提交：HaloPolygon 的視窗像素邊界、BoundingRectViewportUpdate、背景快取；draft_items.py 的點／虛線／完成／取消；草稿存檔保護。前輪 50/50、三種 DPR、實圖像素差 0 見 R09 status；本輪 59 項含其原回歸。
- main 既有 2e8e2b3；R09 先前已由 666d0d1／e28af42 合併。R08／R10 已由 Claude 整合，本輪未更動其來源或進度。
- R08.8 由另一聊天負責；既有 Debug Socket 大量拖曳門檻失敗仍保留，不調門檻、不宣稱已修。R10 已完成；其先前 68/68 C++ 結果不是本輪證據。
- 真實 PCB 仍缺足夠人工資料；同板不同 Z／通道不拆 train/val/test。工作台 v1 與 Socket v3 格式分開。

## 不要動

- .archify/、AiInspTool/cargo/ 來源不明，不清理不 stage；其他聊天室修改不覆蓋。
- 原圖 D:/BMP/PCB、目前程序與使用者最新標註；build/r09-shapes-backup/last-saved.json 僅舊唯讀來源，不能覆蓋最新資料。
- .claude/worktrees/r09-halcon-mask-hint-f0d064 與相關已合併分支的刪除須使用者決定。本輪沒有建立分支或提交。
