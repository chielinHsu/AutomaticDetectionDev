# R09 目前狀態

- 更新日期：2026-11-12；計畫仍進行中，R09.7 真實模型品質驗收未完成。細項單一事實來源見 [plan.md](plan.md)，歷史證據見 [changelog.md](changelog.md)。
- R09.25（2026-11-12，Claude Code）：使用者錄影回報拖角點時藍框與白色把手分離。根因是把手用 Qt 內建的可移動拖曳，會連同已選取的框一起平移；改由把手自行處理拖曳。使用者同意後已提交於 `fix/r09-handle-frame-sync` 並以 no-ff 合回 `main`（未 push）；全套 60/60，Qt Creator 輸出的 payload 已同步該檔，使用者需存檔後重啟工作台才生效，尚待使用者實際操作確認。
- 使用者已授權本機提交與合併。R09.18–R09.23 的 30 個程式／測試／文件以 `b167028` 提交至 `codex/r09-dependent-inspection`，再以 `bf239a4` 非快轉合入 `main`；合併樹與來源分支完全相同。R09.24 已完成，沒有 push。
- 本輪 PARTIALLY VERIFIED：Python／Qt／ONNX 59/59（197.318s），`build/r09-main-merge-20261112/tests.log`；獨立 payload build 28 檔成功，`build/r09-main-merge-20261112/payload`。依賴 316 檔 0 違規、STE 1114 句 0 errors、結構檢查 OK（routing 68/68、10 個既有 architecture 路徑 warnings）、git diff --check 通過。
- R09.18–R09.20：矩形／角點殘影修正、靜態影像背景快取、輪廓點位／虛線／完成按鈕／Enter／起點閉合、Backspace／Esc、較大把手命中與草稿保存／切資料／關閉保護已收進主線。上一輪像素 DPR 與實圖差 0 的證據在 changelog，本輪含原 59 項回歸，未重跑全部 DPR 人工驗收。
- R09.21–R09.23：元件定位 → 本體 → 導腳 → 相依檢測窗已收進主線。第 3 步選參考標註，可用雙端／八腳起始版型，或畫本體、任意腳數與子窗。清單數值改相對父框 X/Y/W/H/angle、刪子樹、Undo／Redo；同封裝同步，父框平移／旋轉帶動子框，尺寸不縮放子框。ADR-044。
- OBB／segment／手動 CAD 都套整組。新 profile.inspection 保存局部框圖，配方 inspection_boxes 含世界座標及親子 ID，重開由最新 pose/profile 重算；舊 v1 沒有群組時保留 window_ratio。模型載入保留目前教導規格；資料匯出只以元件為學習標註。
- 使用者現有視窗沒有被操作或關閉，也不會熱載入。請先完成／取消草稿並 Ctrl+S 儲存，再重啟。10/30 已更新 Qt Creator 的 AiInspTool/bin/vision-workbench，來源比對及兩模式 native --check 通過是當時證據；本輪不重開使用者視窗，另外重新建置 payload 通過。
- 入口：StartCadTools.cmd，2=OBB、3=實例分割、1=原 Socket；Qt Creator 開 AiInspTool/AiInspTool.pro 選 VisionCadWorkbench 執行目標。通用元件影像／自訂封裝類別，PCB 只是目前範例。操作見 [README](../../../../tools/pcb_dl/README.md)。
- 環境：build/pcb-dl-env，Python 3.12；requirements-lock.txt。程序隔離使用 PySide6／Ultralytics／ONNX Runtime，不嵌入 C++。標註 pcb-learning-project v1、配方 pcb-dl-cad v1，與 Socket aoi-recipe v3 分開。
- 真實資料 D:/BMP/PCB：3 組 Z、12 圖，全部 5120×4096；RGB 與三個角度通道逐像素一致是先前證據。保守歸同板來源 pcb-capture-001，不能拆 train/val/test。仍缺足夠獨立板子／批次及人工標註。
- 雙模型先前僅以 12 張合成圖、1 epoch 完成訓練／評估／ONNX 匯出 smoke，test mAP 都為 0，不能當實際 PCB 可用模型。本輪沒有正式訓練或品質驗收。
- 實圖示範 build/r09-inspection/ic-demo-with-inspection.json：1 本體＋8 導腳，雙主題與 1366×768 已檢視；原圖與原標註 SHA 不變。這是起始幾何，非正式預測或人工框位驗收。先前 5000 元件展開 45000 框約 422–1174ms，只是純座標運算，沒有 GUI／產線預算。
- 尚未驗：正式模型精確率／召回率、GPU、產線延遲、完整視窗高 DPI 人工可用性。軸向模 180°不代表 Pin 1／極性；沒有新增獨立 Body／Lead 定位或焊接 OK/NG 演算法。
- 既有跨工作限制：前次 C++ Debug Socket 大量框拖曳門檻失敗尚未修；本輪沒有改 C++／Rust 來源或重跑完整 C++ 全套。前次 Release／Debug 結果及 skip／XFAIL 見 changelog，不當成本輪證據。
- 保留 `.archify/` 與 `AiInspTool/cargo/`，來源不明不提交。原圖、使用者標註、現有程序及舊 worktree 保留；不以 build/r09-shapes-backup 的舊存檔覆蓋最新資料。

下一步是 R09.7：以足夠獨立來源完成人工標註、正式訓練與獨立品質驗收；本輪不自動開始。
