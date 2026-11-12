# Vision CAD Studio — 元件深度學習 CAD 工作台

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| 2026-11-12 | Claude Code (Claude Fable 5.2) | R09.26：第 2 步的幾何欄位即時更新畫布上的矩形框。 |
| 2026-10-30 | Codex (gpt-6.1-sol) | 通用元件三欄工作台、雙主題與 Qt Creator 聯合建置。 |

工作台可處理使用者自訂的元件影像與類別，PCB 是目前的範例資料，不是輸入限制。產品名稱使用 Vision CAD Studio；內部 tools.pcb_dl 路徑及既有 JSON 格式保留相容。

## 從 Qt Creator 建置與執行

1. 開啟專案根目錄下的 `AiInspTool/AiInspTool.pro`，選 Desktop Qt 6.12.0 MSVC2022 64bit kit，使用 shadow build。
2. 執行 **Run qmake**（新增子專案後需重新載入），再 **Build Project**。預設會建置原 `AiInspTool.exe`、測試與新 `VisionCadWorkbench.exe`。
3. 在左下角執行組態或 **Projects → Run → Run configuration** 選 **VisionCadWorkbench**；原工具選 **AiInspTool**。
4. 工作台預設 OBB。需要實例分割時，在 Run 的 **Command line arguments** 填 `--task segment`；OBB 可填 `--task obb`。
5. 接續標註可加 `--project "D:/path/to/project.json"`。不要以舊備份覆蓋仍在另一視窗編輯的資料，先在該視窗明確儲存。

輸出在 `<Qt Creator 建置資料夾>/bin/VisionCadWorkbench.exe`，應用程式檔案在相鄰的 `vision-workbench/`。建置包含 Python 3.12 x64／套件 metadata 檢查、語法檢查及 bytecode 編譯。模型／影像／既有標註不會複製進建置包。

Python 預設使用專案的 `build/pcb-dl-env/Scripts/python.exe`。第一次未安裝時，先執行下方 setup 指令。也可在 qmake 的其他引數填 `AOI_PCB_PYTHON=C:/path/to/python.exe`，指定已具備依賴的環境。一般 build 不下載套件、不安裝模型。

只需原 C++ 工具時可加 `AOI_BUILD_PCB_WORKBENCH=OFF`；此選項名稱是內部相容命名。工作台維持獨立 Python 程序，不需改用另一個 Qt Creator 專案。執行環境依賴本機 Python 與 Qt kit，並非免安裝單檔產品；Python 環境移動後須重新指定並建置。

命令列 smoke 檢查：先 dot-source `scripts/dev/env.ps1`，再執行建置的 `VisionCadWorkbench.exe --check --task obb` 或 `--check --task segment`。輸出 `status=ok` 與 build payload 的 module 路徑才表示該產物成功啟動。

## 新版工作台

### 畫矩形、畫輪廓與完成

- 矩形：按住左鍵拖曳，放開建立一個標註。Esc 取消尚未放開的草稿。
- 輪廓：每按一下放置一個可見點；綠點是起點，其他點為白色，虛線是游標的下一段預覽。
- 至少放三個不同點後，可按畫布上方 **完成輪廓 (Enter)**、Enter／數字鍵盤 Enter，或按綠色起點閉合。Backspace 退回一點，**取消 (Esc)** 放棄草稿。
- 有自交、退化或越界時，點位留在草稿並顯示原因；沒有新增標註。成功時顯示「輪廓已完成」，切回選取並顯示白色頂點把手，左側標註數增加。
- 草稿會顯示「尚未存入標註」，需先完成或取消，才能儲存／換資料／關閉。完成後按 Ctrl+S 保存。
- 調整輪廓：選取它，拖動白色頂點；可點範圍已加大，滑鼠停留會變色。點太密時先用滾輪放大；拖整個框時把手會一起移動。

### 選取和平移的差別

| 工具 | 在元件／把手上拖曳 | 在空白影像上拖曳 |
|---|---|---|
| 選取 | 移動元件或修改頂點 | 平移影像 |
| 平移 | 平移影像，標註坐標保持不變 | 平移影像 |

因此兩者從空白處拖動時相同；要從密集元件區域移動視野時，平移可防止誤拖框。

- 左側：圖片縮圖、類別選擇、依類別收合與查詢的標註清單。
- 中央：影像、縮放／適配、選取／平移／矩形／多邊形工具，只有一個工具啟用。
- 右側：選取項名稱、類別、形狀、中心 X/Y、W/H、角度與同一元件的裁切預覽。矩形的數值一變（上下箭頭、滾輪、輸入後按 Enter 或 Tab）框與白色把手就立刻平移、縮放、旋轉，不必按〔套用矩形幾何〕；同一欄位的連續調整合成一筆 Undo。超出影像、相依檢測框超出影像或寬高小於 2 px 的值不會套用，框維持上一個有效狀態並在上方提示。〔套用矩形幾何〕保留：重新檢查目前的值，並把欄位整理成長邊為寬、角度 -90°～90° 的表示。多邊形用頂點編輯。
- 右上角選深色／淺色；首次深色，之後記住偏好。切換不更動原圖、標註、草稿、選取或縮放。
- 同類使用固定顏色，選取另有白色外框與把手。「幾何有效」不等於已人工審核整張圖；實際漏標仍需人工檢查。

從專案根目錄雙擊 `StartCadTools.cmd`，選 **2：OBB** 或 **3：實例分割**。選 1 可回到原 Socket 規則版。深度學習使用獨立 PySide6 工作視窗，不影響原 C++ Qt 工具。

目前沒有使用者 PCB 的標註或權重。未載入模型時，工作台只提供人工標註；不會用 Socket 規則冒充 AI。電阻、電容、IC 是初始類別，可新增更細的封裝類別，例如 `resistor_0402`。不同封裝若要不同 CAD 規格，應使用不同類別。

介面已改為上方五步引導。第一次請依「載入圖片 → 標註元件 → 設定檢測框 → 訓練模型 → 產生與儲存」操作；完整圖文步驟見 [新手操作說明](../../docs/steps/R09-deep-learning-cad/ux-walkthrough.md)。框線固定螢幕像素，選取為白框；標註時先選類別，再按「開始畫矩形框」，放開滑鼠後會顯示新增編號。畫一個框只建立一個標註，訓練完成後才可自動搜尋。

元件清單依類別收合，支援搜尋類別／名稱／#編號、雙擊放大定位及全部展開／收合；搜尋同步篩選框線。標籤預設只顯示選取項。矩形角點會保持直角，可明確切換為多邊形自由編輯；舊框不會在開啟時自動改座標。

## 第一次操作

### 本體與導腳相依檢測框（R09.21–R09.23）

元件外框供辨識定位；紅框才是要檢測的區域。原本只有一個比例子窗，現在可設定「元件定位 → 中央本體 → 導腳 → 其他檢測窗」。

1. 在第 2 步框住完整元件。在第 3 步點選參考元件，按「從目前選取的元件帶入尺寸」。
2. IC 可按「建立雙排八腳起始框」，立即產生本體＋8 個導腳；電阻／電容可選雙端起始框。這只是可調整的版型，請依實際影像確認。不同封裝請建立獨立類別，例如 ic_soic8、ic_qfp32。
3. 可直接「畫／重畫中央本體」，再用「增加一個導腳框」逐一教任意腳數。選清單中的導腳後，可「增加相依檢測窗」，例如該腳的焊錫區。
4. 在檢測框樹選本體或導腳，往下捲動，修改相對父框 X/Y、W/H、角度，按「套用」。本體中心／角度改變時，其下所有導腳與檢測窗一起跟隨；修改某一導腳只帶動該腳的子窗。尺寸不自動縮放子框。刪父框同時刪子樹，Ctrl+Z／重做可還原整組修改。
5. 同封裝標註立即預覽相同框組；Ctrl+S 將設定存入標註專案。訓練資料仍只教辨識元件，群組保存在來源專案與模型 profiles。載模型會保留目前自行教導的框組。第 5 步的 OBB／分割／手動 CAD 都套整組框，移動或旋轉元件會一起移動；存配方含每個檢測框的父框 ID 與展開座標。

舊專案沒有框組時維持原子窗比例。這次不包含焊接 OK/NG 演算法；導腳 ID 是編輯識別，並未從影像確認實體 Pin 1／極性。自動辨識品質仍需要自己的正式訓練與獨立驗收。

實圖唯讀副本示範在 `build/r09-inspection/ic-demo-with-inspection.json`；原標註沒有被覆蓋。實作與驗證決策見 [ADR-044](../../docs/adr/044-dependent-inspection-frames.md)。

1. 載入多張 PCB 影像。每張設定來源板子／拍攝批次 `group`，同一板子的不同裁塊／視角必須使用相同 group。
2. 選類別，以矩形或輪廓標註每個完整元件。輪廓逐點點擊後按 Enter；選取後可拖移，黃色頂點可調整輪廓。可刪除、復原、重做。標完整張後勾「已人工審核」；修改標註會取消審核狀態。空圖也要人工審核。
3. 填每類 **CAD W/H（像素）** 與子窗比例。這是檢測規格，不是每個網路偵測框的尺寸。請按實際封裝／解析度設定，不能單憑 resistor 類別推得實體尺寸。
4. 以不同來源 group 分配 train、val、test；至少各一個獨立 group。按「匯出 OBB + 分割訓練資料」，選空目錄。匯出會檢查重複圖、類別、越界、退化／自交輪廓、未審核圖與 group 洩漏。此工具不會自動驗證使用者填的 group 是否真實。
5. 選 `obb` 或 `segment`，按訓練。選剛才匯出的根目錄與輸出目錄。完成後依序做獨立 test 集評估與 ONNX 匯出，產出 `<輸出>/<task>/deployment/model.json`。兩種模式要各訓練一次。預設從隨機初始化開始，CPU 可執行但正式訓練建議配置足夠運算資源；CLI 可指定 `--device 0` 使用可用 GPU。
6. 載入對應 `model.json`，可在表格調整各類 CAD 規格，再按全圖推論，或切「推論 ROI」後拖區域。W 沿觀察輪廓的長軸，H 沿短軸。工作台按重疊 tile 推論、還原座標、同類旋轉 IoU 去重，再套固定 CAD 規格。tile 必須大於完整元件；跨 tile 的截斷輪廓會被排除。
7. 在 CAD 配方顯示中，選框拖移，或填 x/y/w/h/angle 後套用；也可按「手動 CAD」拖出定位範圍，尺寸採目前類別規格。單框尺寸不會改其他框；人工框在重新推論時保留。綠框為 CAD、青框為子窗、粉色為模型觀察輪廓。信心分數需要用獨立資料評估，不能直接當合格率。
8. 儲存 **PCB 深度學習 CAD 配方**，可由同一工作台開啟繼續編輯。影像只保存路徑，原圖須保留。此格式為 `pcb-dl-cad` v1，與 Socket 的 `aoi-recipe` v3 分開，不能在 Socket 編輯器載入。標註專案另存 `pcb-learning-project` v1。

## 安裝與命令列

若來源為同一板子的多角度 R/G/B 灰階及合併 PNG，可先建立安全分組的起始專案：

```powershell
build/pcb-dl-env/Scripts/python.exe -m tools.pcb_dl.capture_import D:/BMP/PCB D:/my-annotation-empty --group pcb-capture-001
```

此命令逐像素核對合併通道，把所有 Z／通道視圖歸同一來源，只將合併 PNG 加入標註專案。`project.json` 尚未標註／審核，CAD 尺寸留空。不得把同板不同通道或 Z 分成獨立 train/val/test；需要其他來源板子／批次才可作正式品質驗證。輸出目錄須為空且在原圖目錄之外。

Python 3.12 x64。隔離環境放 `build/pcb-dl-env`，不修改全域 Python。安裝需要網路及磁碟空間。

```powershell
powershell -ExecutionPolicy Bypass -File scripts/dev/setup-pcb-dl.ps1 -PythonExe C:/path/to/python.exe
build/pcb-dl-env/Scripts/python.exe -m tools.pcb_dl.cli gui
build/pcb-dl-env/Scripts/python.exe -m tools.pcb_dl.cli dataset project.json D:/dataset-empty
build/pcb-dl-env/Scripts/python.exe -m tools.pcb_dl.cli train D:/dataset-empty obb D:/training --epochs 100 --device cpu
build/pcb-dl-env/Scripts/python.exe -m tools.pcb_dl.cli train D:/dataset-empty segment D:/training --epochs 100 --device cpu
build/pcb-dl-env/Scripts/python.exe -m tools.pcb_dl.cli infer D:/training/obb/deployment/model.json board.png result.json --roi 0 0 1000 800
build/pcb-dl-env/Scripts/python.exe -m unittest discover -s tests/pcb_dl -v
```

既有訓練權重可用 `cli export <best.pt> <dataset> <task> <output>` 重新評估、匯出。只使用自己或可信訓練流程的 PyTorch 權重；正式推論只載入 ONNX，並檢查 manifest SHA-256、task、類別與 CAD 規格。預設架構 `yolo11n-obb.yaml`／`yolo11n-seg.yaml`，不會下載 COCO／DOTA 預訓練權重並把原類別當 PCB。

## 驗證界線

`tools.pcb_dl.demo` 產生明確標為 `demo_only` 的合成資料，用來跑短訓練、評估、匯出接線。測試中的固定 ONNX 只測還原與去重，不是學習模型。兩者都不能用來宣稱實際 PCB 辨識率。真實品質需要足夠且涵蓋板型、光源、角度、封裝的人工標註，以及未參與調參的板子／批次測試集。

旋轉角為影像座標下逆時針度數，軸向模 180°；不代表 Pin 1／極性。此工具生成檢測視窗，不執行焊接 OK/NG、相機標定或高度量測。

依賴使用 Ultralytics 發行授權；本機開發安裝不代表已處理產品散布的授權選擇。參考官方 [OBB](https://docs.ultralytics.com/tasks/obb)、[Segmentation](https://docs.ultralytics.com/tasks/segment)、[授權](https://www.ultralytics.com/license)。
