# ADR-041 通用元件工作台與 Qt Creator 聯合建置

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| 2026-10-30 | Codex (gpt-6.1-sol) | 依使用者要求將雙主題概念實作，並將工作台加入 AiInspTool.pro；產品用語不限 PCB。 |

- 狀態：採用。
- 決定者：使用者（實作、同一 .pro 建置、通用元件用語）；Codex (gpt-6.1-sol)（程序與建置接線）。
- 相關：ADR-018、ADR-037、ADR-039、ADR-040。

## 背景與證據

原先 StartCadTools.cmd 直接執行 tools.pcb_dl.cli，AiInspTool.pro 只建置 C++ 程式與測試。ui_theme.py 只依 palette 判斷主題；guided_ui.py 以長側欄提供標註操作。使用者要求把概念實作，Qt Creator 開同一 .pro 即可建置兩個工具，並說明實際影像也可能是其他元件。

## 決定

- 使用 Vision CAD Studio／元件深度學習 CAD 的通用顯示名稱，保留 tools.pcb_dl 與既有 JSON format/version，避免破壞資料。元件類別可自訂，不限預設電阻／電容／IC。
- 由 WorkbenchPresentation 與 workbench_ui 建立三欄，既有 Workflow、Canvas、ObjectBrowser 繼續管理標註、資料與 Undo。中心座標／尺寸的矩形修改需完整通過幾何與邊界檢查後，才形成一筆歷史；多邊形繼續明確編輯頂點。
- 以視窗自己的 QPalette、QSS 與 QSettings 管理深／淺色；首次深色，之後記住偏好。不重建 scene 或標註、不轉換照片色彩。類別色在 canvas_items 共用，選取另加白色邊緣與把手。
- 20MP 量測暴露同步縮圖解碼的等待路徑；thumbnail_loader 使用獨立上限兩執行緒的 pool，透過 queued signal 回主 UI 設定圖示，目前圖片直接沿用 canvas 的 pixmap。視窗關閉／銷毀取消尚未讀取的工作，不讓其他圖片縮圖阻擋主畫布載入。
- AiInspTool.pro 預設納入 workbench/workbench.pro，產出 bin/VisionCadWorkbench.exe。PRE_TARGETDEPS 每次執行 build_workbench.py：驗證 Python 3.12 x64 與套件 metadata、檢查語法、複製應用來源、編譯 bytecode，最後寫入 bin/vision-workbench/runtime.json。缺環境／語法錯誤時 build 失敗。
- 原生 Qt launcher 透過 QProcess 啟動 payload 中的模組，以參數列表傳遞 task／project；清除 native kit 的 plugin path 與外部 PYTHONPATH/PYTHONHOME，不經 shell。訓練／推論仍在原本 Python 程序邊界，不將 Torch 嵌入 C++。
- 一般建置離線、不自動 pip 或下載權重。AOI_PCB_PYTHON 可指定已安裝依賴的 Python；AOI_BUILD_PCB_WORKBENCH=OFF 可只建舊工具。這是同一 Qt Creator 專案中的可建置／執行目標，不是 Python runtime 的免安裝單檔散布包。

## 替代方案與後果

- 不將完整標註／訓練流程重寫 C++：那會複製成熟的 Python 管線，且改變 ADR-037 的所有權與依賴邊界。
- 不在每次 build pip install：維持可離線、明確失敗與不修改全域環境。首次環境由現有 setup-pcb-dl.ps1 建立。
- 不只提供 Qt Creator 外部命令：真正加入原生 executable target 與 payload prerequisite，Qt Creator 可選擇執行 VisionCadWorkbench。
- 產物仍依賴 manifest 指向的本機 Python 環境。移動／刪除環境後須重新指定並建置；同一主機的模型／標註檔案不改位置。Qt DLL 的執行環境由 Qt Creator kit 或 scripts/dev/env.ps1 提供。

## 驗證方式

完整 Release 重現交接文件已知的來源樹 `.moc` 污染：qmake 將測試 obj 的依賴連到 AiInspTool/tests 的舊生成檔，編譯卻只 include shadow 的 MOC_DIR。為維持新資料夾可建置，aoi_test.pri 對 tst_／perf_ 主測試物件補上同一 shadow moc 的明確相依；保留原標頭依賴掃描，不移除其他聊天的生成檔。純工具 record_guidance 不加入不存在的 moc 相依。首個絕對路徑規則未匹配 Makefile 的相對物件路徑，改用同一相對 Windows 路徑後單例成功，完整結果另記。

Python／Qt／ONNX 回歸、雙主題下狀態保持、實際 Qt 畫面；qmake／jom Release、Debug、Profile 與關閉工作台選項；launcher --check 檢查兩種 task 並確認 module 位於 build payload。建置與測試實際結果記在 R09 的 status／changelog 及 perf.md，不以本決策代替驗證。
