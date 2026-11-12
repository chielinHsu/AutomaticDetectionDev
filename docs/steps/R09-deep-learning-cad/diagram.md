# 資料與推論流程

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| 2026-10-30 | Codex (gpt-6.1-sol) | R09.18–R09.20：殘影、背景快取與輪廓草稿／完成交易。 |
| 2026-10-30 | Codex (gpt-6.1-sol) | 加入 Qt Creator 建置與主題／選取流程。 |

## 畫布殘影與輪廓完成（R09.18–R09.20）

修改前：

```mermaid
flowchart LR
 A[Canvas.mouseMoveEvent／Vertex.mouseMoveEvent] --> B[HaloPolygon.paint 固定 6／7px]
 B --> C[原 polygon 邊界按 3 個場景單位計算]
 C --> D[縮小後更新不足／小數 DPR 分段更新]
 D --> E[舊像素需全重畫才消失]
 F[影像 pixmap 每個覆蓋層髒區重取樣] --> G[實圖部分行列取樣差異]
 H[輪廓只有黃色連線] --> I[Enter 前無可見點位或完成動作]
```

修改後：

```mermaid
flowchart LR
 A[Canvas 縮放／建立覆蓋層] --> B[HaloPolygon.set_view_scale 換算 padding]
 B --> C[prepareGeometryChange 與 BoundingRectViewportUpdate]
 C --> D[局部像素與完整重畫一致]
 E[drawBackground／CacheBackground] --> F[影像一次取樣、覆蓋層獨立更新]
 G[ContourDraft 點位／開放折線／虛線預覽] --> H[完成按鈕／Enter／點起點]
 H --> I[finish_polygon 幾何驗證]
 I -->|有效| J[Workflow.drawn 一筆 Undo、切選取與白色頂點]
 I -->|無效| G
 G --> K[草稿標示與儲存／關閉保護]
```


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

## 相依檢測框修改前後（R09.21–R09.23）

```mermaid
flowchart LR
 A[原 Workflow.drawn 元件分類標註] --> B[LearningEngine.predict 定位 pose]
 B --> C[原 Canvas.render 單一比例子窗]
```

```mermaid
flowchart TD
 A[InspectionEditor 本體／導腳／檢測窗教導] --> B[profile.inspection 局部框圖]
 B --> C[驗父框／循環／幾何／全專案影像邊界]
 C --> D[一筆 Undo 交易及同封裝同步]
 E[OBB／segment／手動 CAD pose] --> F[inspection_boxes 相依展開]
 B --> F
 F --> G[component → body → pin → window]
 G --> H[Canvas.render 紅色檢測框]
 G --> I[make_recipe 含親子 ID 與世界座標]
 I --> J[重開由最新 pose／profile 重算]
```

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
