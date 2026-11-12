# 2026-11-12 — 相依檢測框與畫布修正提交並合回 main（Codex / gpt-6.1-sol）

- 使用者明確要求「先合併回主線」，授權本機提交及 no-ff 合併，不包含 push。新增 R09.24，沒有開始 R09.7 或其他計畫。
- 目前 main 的未提交差異均屬 R09.18–R09.23，包含前輪殘影／輪廓修正與本輪相依本體／導腳／檢測窗。精確 stage 30 個檔案，核對沒有多餘／缺漏路徑及未 stage 的追蹤差異。
- 來源提交 `b1670280803b1e8447429744f27b7d4e9edbcbd5`（codex/r09-dependent-inspection）；非快轉合併 `bf239a4a6bd1522b61a59aaa603d443e6f49ebba`（main）。作者與 committer 均 Caspar Hsu <chielin0025@hotmail.com>。git diff 來源分支與合併樹 exit 0，合併有兩個 parent，沒有衝突。
- 本輪重跑 Python／Qt／ONNX 全套 59/59，197.318s；build/r09-main-merge-20261112/tests.log。獨立 payload builder 28 檔成功，未操作／關閉使用者工作視窗；不重編未改的 C++ launcher。
- 依賴 316 檔 0 違規、STE 1114 句 0 errors、結構檢查 OK（routing 68/68，10 個既有 architecture 路徑 warnings）、git diff --check 通過。靜態檢查與 stage scope／來源 SHA 記錄在同一 ignored 目錄。
- 更新 plan 的 R09.18–R09.24 提交狀態、重整 status 為現在快照、覆寫 handoff。計畫仍進行中，沒有修改 INDEX 的計畫狀態；正式品質 R09.7／GPU／Pin 1／極性／產線 OK/NG、完整 C++／Rust 全套與高 DPI 人工驗收仍未做。
- .archify/ 與 AiInspTool/cargo/ 不 stage，合併前保留其檔案數及 SHA 清單供合併後核對；原圖、使用者標註與其他工作不更動，沒有 push。

# 2026-10-30 — 相依本體／導腳／檢測窗與整組自動編著（Codex / gpt-6.1-sol）

- R09.21–R09.23：依使用者 IC 截圖與三份本機 TRI PDF 建立相依檢測框。IC 表格及 Link 頁已渲染檢視；文件只是參考資料，未執行其操作指示。
- 新增 inspection.py（局部剛體轉換、框圖／循環／尺寸驗證、兩種起始版型、邊界、世界座標展開）、inspection_editor.py（第 3 步本體／導腳／巢狀窗教導、樹清單數值修改、刪子樹、共享規格與原子 Undo）。
- model／inference／workflow 以最新 pose/profile 產生整組，配方另存帶親子 ID 的 inspection_boxes；標註／資料集／模型保留 optional inspection，舊 v1 window_ratio 保留。sync_profiles 不再丟掉新增欄位；載模型保留目前教導規格；history 增加 profiles；所有幾何修改加子框邊界檢查。
- window／canvas／guidance／guided_ui／workbench_presenter 顯示紅色檢測框，與外側分類／定位框分開；拖元件和頂點預覽時檢測區跟隨。第 3 步清單可查選參考元件，工具與草稿流程沿用，非法繪製移除草稿而保留原資料。
- 新增 test_inspection_groups.py 9 項；初輪專項 8/8，補資料匯出後最終全套 59/59、60.354s，build/r09-inspection/regression-delivery.log。涵蓋兩 task 真 ONNX Runtime fixture 9 區域、滑鼠畫本體＋8 腳＋子窗、旋轉／偏移、Undo／Redo、存讀、舊檔、循環／越界與錯誤原子性。
- payload builder 成功（28 檔），確認 runtime 設定相等後原子更新目前 Qt Creator 輸出 10 個 Python 檔，設定雜湊不變、全來源逐檔一致；兩模式 native --check exit 0。未關閉或操作使用者目前視窗，使用者需先存檔再重啟。C++ launcher 未改，不重建未改的 C++／Rust。
- 實圖示範獨立建立 1 本體＋8 腳，dark／light／1366×768 已檢視，原圖與原標註 SHA 不變。5000 元件展開 45000 框，第一次 1088–1174ms、最終 422–619ms，負載不同，無等價舊基準，不稱效能改善／p95。preview*.log、real-preview-and-perf.json 與 ic-related-*.png 在同目錄。
- check-deps 316 files／0 violations；STE 1114 句／0 errors；結構檢查初次被 sandbox Git ownership 擋下，單次 GIT_CONFIG_COUNT safe.directory 重跑 OK（routing 68/68，10 既有路徑 warnings）；未改全域設定。互動說明頁 5 scripts 語法、3 次控制項 render／33 個矩形通過；git diff --check 通過。
- 新增 ADR-044／索引，更新四種輸出、perf、README、計畫／status／handoff。R09.21–R09.23 完成實作，PARTIALLY VERIFIED；R09.7 正式模型品質／GPU／Pin 1／極性／產線 OK/NG 與完整高 DPI 人工驗收仍未做。沒有 stage／commit／push，原有 R09.18–20 與來源不明未追蹤資料保留。

# 2026-10-30 — 殘影修復與輪廓建立／編輯回饋（Codex / gpt-6.1-sol）

- R09.18–R09.20：使用者回報畫矩形、拖角殘影，並詢問選取／平移差異；後續補上輪廓無點位、難調整、不知道何時完成的截圖。
- 先建立真實滑鼠與背板像素的 red loop：9／15／24% 的拉角留下 102／217／427 像素；去掉裝飾仍重現，補畫筆邊界對照即歸零。原 HaloPolygon 只登記 3 場景單位，實際畫固定 6／7px；修正比例 padding 與 prepareGeometryChange，合併髒矩形處理小數 DPR。
- 實圖再抓到背景局部重取樣的行列跳動，雜訊 fixture 可重現 384 像素，平滑取樣仍有差。把 pixmap 移至 viewport 背景快取、載圖重設快取，保留原圖資料與坐標；實際 20MP 矩形、拉角、輪廓預覽原生背板對照全部 0，原存檔 SHA 不變。
- 新增 draft_items.py／ContourDraft，開放折線、固定點、綠起點與獨立虛線預覽；完成／Enter／數字 Enter／按起點共用驗證，無效保留點位。完成後切選取，明確訊息與白色把手；Backspace 退點、Esc 取消。把手視覺 12px、命中 18px、hover 與拖框跟隨。
- guidance／workflow 加草稿狀態、未完成時的儲存／切資料／關閉保護；工具提示及 README 說清選取可拖框／空白平移，平移可從框上拖影像且不改標註。model、格式、C++／Rust 與訓練程式未變。
- 新增 test_canvas_repaint.py、test_contour_interaction.py，修改原把手尺寸斷言。全套 50/50，78.754s；5 項像素回歸在 100／150／200% DPR 通過。原 QScreen 抓圖在小數 DPR 截短，改直接讀完整 backingStore QImage；測試的 Python 半偶數 rounding 與 Qt 半入 rounding 差異也已校正，沒有放寬像素容忍。
- jom workbench_payload 曾被活躍啟動器的 runtime.json 讀鎖擋下（WinError 32）；沒有關閉／殺程序。獨立 ready 目錄建置成功，確認執行設定等價後只原子替換程式檔、重建 bytecode；設定不動，來源比對一致，實際 Qt Creator 輸出的兩種 task native 啟動通過。現有視窗仍是舊載入內容，待使用者存檔重啟。
- 新增 ADR-043 與索引，更新 R09 計畫／status、diagram／perf／README 及交接。完整 C++、GPU、正式訓練與整個視窗高 DPI 人工驗收本次未跑；結果 PARTIALLY VERIFIED。這輪不 stage／commit／push，其他計畫工作保留。

# 2026-10-30 — R09 本機提交與主線整合（Codex / gpt-6.1-sol）

- 使用者明確要求「合併回主線」，授權本機 commit 與 merge；本次範圍為 R09 工作台、概念／規格、Qt Creator 整合與相關建置相依。
- 以 codex/r09-vision-workbench 整理本次提交，採 --no-ff 合入 main；R08／R07 與來源不明修改保留，不 push。共同文件只納入 R09 對應段落。
- 合併不變更功能或測試門檻；沿用上輪 37/37、5/5 專項、6/6 啟動、Release 67/67 與 Debug 66/67 的實際結果。Debug Socket 拖曳失敗、8 組 skip／2 XFAIL 與未驗高 DPI／模型品質仍保留，狀態不升為 VERIFIED。
- 來源提交 `abe86ef500b3`、非快轉合併 `666d0d15d615` 已完成，main 的合併樹與來源樹一致。隔離暫存快照的 5 項測試（11.391s）、qmake／jom 工作台建置、OBB／segment 兩項 native 啟動通過。其他追蹤中差異及未追蹤路徑雜湊合併前後一致；臨時暫存的兩份共同文件已還原，僅移除本次建立且已驗證還原的 stash。
- 本輪未重跑原 C++ 整套；先前 67/67／66/67 是共用工作目錄的結果，包含仍未提交的其他工作，不能宣稱隔離主線已重新通過整套。R09 計畫仍進行中，R09.7 未完成。

# 2026-10-30 — 實作通用元件工作台並整合 Qt Creator 建置（Codex / gpt-6.1-sol）

- R09.15–R09.17 實作完成，驗證為 PARTIALLY VERIFIED。使用者先要求將概念落實、同一 AiInspTool.pro 建置兩個程式，再要求產品不限定 PCB。名稱改 Vision CAD Studio／元件深度學習 CAD，資料格式與 tools.pcb_dl 路徑保持相容。
- 新增 workbench_ui／workbench_presenter／workbench_theme／thumbnail_loader；調整 guided_ui、window、guidance、workflow、canvas、canvas_items、object_browser、ui_theme。圖片縮圖／類別清單、中央畫布、選取屬性三欄；預設深色與可記憶淺色；中心座標／矩形幾何交易、裁切、類別色、白色選取把手、草稿保護與單一導覽／工具狀態。名稱與 source group 文案通用化，screw 類別實測。
- AiInspTool.pro 加入 workbench/workbench.pro、build_workbench.py 與 WorkbenchLauncher.cpp，產出 VisionCadWorkbench.exe 及 Python payload；提供環境覆寫、停用選項與 --check，平常 build 不下載。原 C++ 與新工作台 Release／Debug 建置成功；工作台 Profile 成功（原 C++ Profile 本次未整套重建）。三種產物逐檔比對 Python 源碼，兩種 task 的 native 啟動 6/6 exit 0。檢查腳本的重複 task 參數錯誤已修正並重跑。
- 新增 test_workbench_shell.py 五項核心互動測試，修改原框線像素與空狀態文案斷言。全套 37/37（164.608s）；最後導覽／平移與字形修正後 5/5 專項（5.395s）。19 組實際 runtime 色票對比通過。三張實際畫面在 build/r09-workbench-*.png，20MP／8 框、1366×768 與原檔 SHA-256 不變已查核；背景縮圖與切換／refresh 量測記在 perf.md，非正式預算。
- 原 C++ 初次新 shadow build 因 source moc 污染 C1083，aoi_test.pri 補上 tst_／perf_ 主物件至同一 shadow moc 的明確相依；修正過相對路徑與完整 regex 匹配，沒有刪除他人生成檔。最終 Release 預設建置／回歸 67/67，8 組有 skip、2 個既有 XFAIL（OpenCV low contrast 角度、palette BMP）。
- Debug 預設建置成功，回歸 66/67；Socket 大量框拖曳門檻失敗。整組重跑仍有 1017／5000 兩列失敗。最後僅跑拖曳函式，1017=3201.38ms 失敗、5000=1984.50ms 通過，exit 1。原因未判定，沒有改 Socket 源碼、提高門檻或新增 XFAIL，不宣稱 Debug 全套通過。證據 r09-cpp-debug-regression.log、r09-cpp-debug-solder-recheck.log、r09-debug-drag-isolated.txt。
- 新增 ADR-041，更新其索引、architecture、AI_CONTEXT、README、R09 的 spec／diagram／explainer／storyboard／perf 與計畫快照。STE 1059 sentences、0 errors／warnings；check-deps 314 檔 0 違規；結構 Strict 通過（model routing 68 passed、9 個既有路徑警告）。缺依賴負向測試與停用工作台 configure 通過。
- 未重開使用者未儲存視窗，未更動真實影像／已存檔標註，未正式重訓或宣稱模型品質；未驗 150%／200% DPI，不 stage／commit／push。R09.7 仍待正式資料；R08／R07 與來源不明修改保持原狀。

# 2026-10-30 — 第二階段工作台雙主題視覺概念（Codex / gpt-6.1-sol）

- R09.14：依使用者三張參考與後續「同時支援、預設深色」要求，產出深／淺色兩張 1536×1024 概念。三欄分工、中央畫布、藍色操作、類別固定色、#02 電阻／R22／裁切一致；分開幾何檢查與人工審核。
- 新增 docs/steps/R09-deep-learning-cad/workbench/ 的 DESIGN.md、theme-tokens.json、prompts.md、review.md、annotation-dark.png、annotation-light.png；新增 ADR-040 與索引，更新本計畫與交接。計畫整體仍進行中，INDEX.md 不改。
- 內建 imagegen 生成後逐張檢視；19 組色票對比通過，STE 1049 sentences／0 errors／0 warnings，diff --check 通過。結構檢查首次被 sandbox Git ownership 擋下，當次程序 safe.directory 後重跑成功，68 routing checks、9 個既有路徑警告；未改全域 Git 設定。
- 深色圖 #04 仍有白色角點的生成瑕疵，淺色已移除；規格要求僅選取項有把手。生成圖不當逐像素 UI 契約；PARTIALLY VERIFIED，互動／DPI 未驗。
- 僅概念與規格，不修改 PySide6／C++／Rust，不重跑程式測試或訓練，不重開使用者目前未儲存視窗，不 stage／commit／push。R09.14 範圍內完成；執行介面改版尚未實作。

# 2026-10-30 — 矩形拉伸與類別收合查詢（Codex / gpt-6.1-sol）

- 回應矩形角點會變多邊形：新增可選 shape 意圖與共用幾何，四角保持直角，預覽／提交一致，明確矩形／多邊形切換、Undo 與舊檔相容。
- 回應大量 IC／電容／電阻資訊混亂：左側類別樹預設收合，數量、搜尋／畫面篩選、點選同步、雙擊／Enter 定位、面板切換；可填 name，預設只顯示選取標籤。
- 群組節點不刪元件，搜尋文字 Delete 只編輯文字。新 11 項專項與原 21 項合計 32 項通過；資料不因搜尋收合而改動。
- 真實已存檔資料載入不改任何內容。使用者仍在操作未儲存視窗，工具阻擋輸入後停止重開；不把舊備份覆蓋現有工作，需使用者先存檔再重開。
- ADR-039；新增 annotation_geometry.py／object_browser.py，更新畫布、標註契約與 UI。R09.12／R09.13 完成，無 stage／commit／push。

# 2026-10-30 — Delete 鍵刪除修復（Codex / gpt-6.1-sol）

- 使用者選到框但 Delete 無作用；重現測試中畫布、清單與 CAD 均失敗，文字欄位正常。根因是漏接鍵盤，原本只有刪除按鈕。
- 將 Delete 限定於畫布與框清單，沿用既有刪除與 Undo；未完成輪廓先完成／取消，無選取不動資料，增加可見提示。
- 五項 Delete 回歸涵蓋載圖步驟、清單、CAD、Undo、文字焦點與輪廓草稿；全部 21 項測試通過。沒有改資料格式或 C++／Rust。
- 重開前經原 UI 保存使用者未儲存的框調整；8 個框及備份保留，不在實際標註上試刪。
- R09.11 完成；沒有 stage／commit／push。

# 2026-10-29 — 五步新手 UX 與清楚框線（Codex / gpt-6.1-sol）

- 回應使用者無法理解設定順序與看不見框的回報，重整成五步、漸進設定、前置條件、主動作與明確回饋，加入類別修正及尺寸帶入。
- 1 scene-unit 細線改成固定像素雙描邊、底色編號標籤與選取把手；快取原圖 pixmap。保留資料格式與模型流程。
- 透過原 Windows UI 備份八個標註，逐欄還原無變更；新版已在桌面開啟。Ctrl+S／另存、存檔取消保護與小拖曳提示完成。
- 16 項 Python／Qt／ONNX 測試通過；六項 UX 含全圖與半倍率實際像素。首次像素斷言誤採到標籤，改在框線中段量測後通過，未放寬線寬要求。另以失敗測試鎖定點空白未解除選取的回授問題，阻擋表格清除時的重入選取訊號後修復。
- R09.9／R09.10 完成；實際模型品質 R09.7 仍未驗證。本次未重跑 C++ 全套或訓練。
- 決策 ADR-038；不 stage／commit／push，原 R07／Socket 工作不變。

# 2026-10-29 — 真實 PCB 多通道資料接入（Codex / gpt-6.1-sol）

- 使用者提供 D:/BMP/PCB；盤點 12 張 5120×4096 影像，確認 3 張合併圖的 9 個通道與 BMP 完全一致。
- 新增 capture_import 工具與兩項測試；所有 Z 視圖同 group，僅把合併 PNG 加入未審核起始專案，原圖不動。
- 修正匯入器缺少 R 通道時會略過整組的檢查缺口；正負例、缺檔／合併錯誤及防覆寫測試通過。
- 實際 Qt 工作台載入三張 RGB 成功。只有原始照片，沒有標註／正式模型，不宣稱真實 PCB 品質通過；R09.7 繼續待驗。
- 無 stage／commit／push，保留既有工作。

# 2026-10-29 — 雙模式工作台（Codex / gpt-6.1-sol）

- 新增 tools/pcb_dl：文件契約、資料驗證／雙匯出、ONNX 推論、訓練／評估／匯出、Qt canvas／window／workflow 及 CLI。
- 新增 StartCadTools.cmd、scripts/dev/setup-pcb-dl.ps1、環境鎖定檔、8 項 Python／Qt／ONNX 測試及四種輸出。
- 選擇獨立 PySide6 程序，保留 C++ 五層與 Socket，決策 ADR-037。
- 8 項測試通過；兩種模型各完成短訓練、test 評估及 ONNX 匯出／推論。合成 mAP=0，明確標為 demo_only；不宣稱實際 PCB 品質。
- R09.1–R09.6 技術流程完成；R09.7 真實資料驗收仍未完成。無 commit／push。

# R09 變更紀錄

## 2026-10-29 — Codex (gpt-6.1-sol)

- 使用者指定先完成原工作，再新增 OBB 偵測與實例分割兩種模式。
- 使用者確認 PCB 電阻／電容／IC 等多類元件，沒有現成標註或模型；加入標註與訓練流程範圍。
- 尚未執行本計畫的模型推論或訓練，不宣稱已具備深度學習辨識效果。
