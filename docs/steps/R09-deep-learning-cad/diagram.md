# 資料與推論流程

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| 2026-10-30 | Codex (gpt-6.1-sol) | 加入 Qt Creator 建置與主題／選取流程。 |

## 本次修改前

```mermaid
flowchart LR
 A[AiInspTool.pro] --> B[C++ AiInspTool.exe]
 C[StartCadTools.cmd] --> D[cli.py gui] --> E[guided_ui.build_ui 長側欄]
 E --> F[ui_theme 依系統 palette]
```

## 本次修改後

```mermaid
flowchart TD
 A[AiInspTool.pro] --> B[C++ AiInspTool.exe 保留]
 A --> C[workbench.pro 新增]
 C --> D[build_workbench.py 環境／語法／bytecode／payload]
 D --> E[VisionCadWorkbench.exe 與 runtime.json]
 E --> F[WorkbenchLauncher QProcess]
 F --> G[建置 payload 的 cli.py gui]
 G --> H[workbench_ui 三欄與五步]
 H --> I[WorkbenchPresentation 同步選取／crop／幾何]
 I --> J[既有 Workflow／Canvas／Undo]
 H --> K[workbench_theme 視窗 palette 與 QSettings]
 K --> L[保持原 scene／草稿／選取／影像]
```

```mermaid
flowchart TD
 A[人工輪廓／矩形與類別] --> B[審核、來源 group、train/val/test]
 B --> C[資料驗證]
 C --> D[最小面積 OBB 標註]
 C --> E[Instance polygon 標註]
 D --> F[OBB 訓練／test 評估／ONNX]
 E --> G[Seg 訓練／test 評估／ONNX]
 F --> H[Manifest + SHA + CAD profiles]
 G --> H
 H --> I[ROI 重疊 tiles 真實推論]
 I --> J[全圖座標、軸向、輪廓、類別 NMS]
 J --> K[共享 CAD 規格 + 人工覆寫]
 K --> L[拖移／數值旋轉編輯與配方]
```

模型未載入、task 不符或類別／規格缺失：明確停止生成。人工標註與模型提案分開保存。

## 新手操作引導（R09.9）

```mermaid
flowchart LR
 A[1 載入圖片] --> B[2 選類別、畫框、檢查]
 B --> C[3 選標註帶入 CAD 尺寸]
 C --> D[4 來源分組、匯出與訓練]
 D --> E[5 載入模型、產生並儲存]
 F[已有模型] --> E
 B --> G[編號與固定像素框線、明確新增提示]
 G --> H[選取改類別、復原與儲存]
```

## 類別查詢與形狀（R09.12–R09.13）

```mermaid
flowchart LR
 A[類別群組與數量，預設收合] --> B[搜尋類別／名稱／編號]
 B --> C[同步篩選框線，資料不變]
 C --> D[單擊選取／雙擊放大]
 D --> E{標註形狀}
 E --> F[矩形：保持直角拉伸]
 E --> G[多邊形：自由頂點]
 F <--> H[明確轉換，可復原]
 H <--> G
```
