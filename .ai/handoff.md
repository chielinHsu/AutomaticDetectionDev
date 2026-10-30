# Agent Handoff

- 更新：Codex (gpt-6.1-sol)，2026-10-30；本次 [R09](../docs/plans/active/R09-deep-learning-cad/status.md) R09.15–R09.17 已實作，驗證 PARTIALLY VERIFIED。沒有 stage／commit／push。
- 使用者要求概念落實至程式、同一 AiInspTool.pro 建置兩個工具，並指示不要限定 PCB。產品名 **Vision CAD Studio／元件深度學習 CAD**；模組與 JSON 格式保留相容。

## 本次程式與入口

- Qt Creator 開 AiInspTool/AiInspTool.pro → Run qmake → Build，可選 AiInspTool 或 VisionCadWorkbench 執行目標；工作台參數 --task obb／segment，--project 可接續人工標註。完整操作 tools/pcb_dl/README.md。
- 新增 workbench/workbench.pro、src/app/WorkbenchLauncher.cpp、tools/pcb_dl/build_workbench.py。建置檢查 Python 3.12 x64／套件、語法、複製及編譯 bytecode，輸出 bin/VisionCadWorkbench.exe 與 bin/vision-workbench；一般 build 不 pip／下載。AOI_PCB_PYTHON 可覆寫；AOI_BUILD_PCB_WORKBENCH=OFF 停用。
- 新增 workbench_ui／workbench_presenter／workbench_theme／thumbnail_loader；更新 guided_ui、window、guidance、workflow、canvas、canvas_items、object_browser、ui_theme。三欄、固定類別色、選取屬性／矩形幾何／裁切／人工審核、首次深色與持久偏好。主題不重建 scene 或改標註；背景縮圖限兩執行緒，視窗關閉／銷毀取消剩餘工作。
- 選取／平移／矩形／輪廓互斥；平移點到把手不會轉成編輯。草稿阻擋切步時還原單一導覽高亮；文字欄位 Delete 保護與既有 Undo 保留。
- 新增 test_workbench_shell.py；原 test_guidance.py 的顏色及文案斷言跟隨新設計。使用 screw 類別驗證通用元件，沒有自動改使用者真實標註類別。
- aoi_test.pri 補 tst_／perf_ 主物件到同一 shadow moc 的相依，修正新資料夾 C1083；沒有搬／刪來源樹舊產物。ADR-041 記錄設計；R09 計畫仍進行中，INDEX 沒改。

## 實際驗證

- Python／Qt／ONNX 全套 37/37，164.608s，build/r09-workbench-final.log；最後 UI 狀態修正後 5/5 專項，r09-shell-final.log。源碼與三種 payload 的每個 Python 檔比對一致。
- 工作台 Release／Debug／Profile 建置成功；6/6 native --check 啟動通過，對應 module 都在各自 build payload，r09-launcher-checks.json。原 C++ Release／Debug 建置成功；原 C++ Profile 本次未完整重建。
- 原 C++ Release：67/67，8 組有 skip、2 既有 XFAIL。HALCON backend 實際通過；skip 是缺真實資料／截圖環境、全內建分支或影像格式限制。r09-cpp-release-regression.log。
- **Debug 未全過**：66/67，Socket 大量框拖曳門檻失敗；整組重跑兩列仍失敗。最後純拖曳案例：1017 框 3201.38ms（門檻 2000，失敗）、5000 框 1984.50ms（通過），exit 1。證據 build/r09-debug-drag-isolated.txt、r09-cpp-debug-regression.log、r09-cpp-debug-solder-recheck.log。原因未判定，沒有改 Socket 源碼、調高門檻或新增 XFAIL。
- 20MP／8 框的深淺實際畫面與 1366×768 已檢視、原存檔 SHA 不變。圖 build/r09-workbench-{dark,light,small}.png，量測 r09-workbench-ui-check.json；perf.md 記載非受控負載數字，不宣稱正式 p95／改善倍率。
- 19 組實際 runtime 色票對比通過；check-deps 314 檔 0 違規；STE 1059 sentences、0 errors／warnings；結構 Strict OK（routing 68 passed、9 個既有架構路徑警告）。缺依賴負向與關閉工作台 configure 通過。
- 沒有重訓正式模型、GPU 或高 DPI 150%／200% 驗收；R09.7 品質仍待足夠真實資料。不要把部分通過寫成 VERIFIED。

## 資料與其他工作保留

- 前次交接指出使用者視窗可能仍有未儲存調整；本次完全沒有操作／重開桌面視窗。build/r09-shapes-backup 只有舊已存檔八框，僅拿來唯讀驗證，不能覆蓋現有工作。使用者先存檔，再用新版開啟同一專案。
- 原圖 D:/BMP/PCB、12 圖／3 個 Z 視圖、5120×4096、同板同視野單一來源 group 的規則保留，不可拆 train/val/test 湊數。正式資料需求也適用其他元件。
- pcb-dl-cad v1／pcb-learning-project v1 與 Socket aoi-recipe v3 各自使用；影像仍只存路徑。
- R08／R07、tests/core/tst_generate_objects.cpp、tests/domain/tst_lattice.cpp、.gitignore、docs/ux/copy.json、CompositionRoot／main／MainWindow 及其他既有未提交修改保持原樣；本次只新增 WorkbenchLauncher 到 app。
- .archify/、AiInspTool/cargo/ 等來源不明工作不清理、不 stage；不同聊天共用來源，不結束其他視窗／行程。
